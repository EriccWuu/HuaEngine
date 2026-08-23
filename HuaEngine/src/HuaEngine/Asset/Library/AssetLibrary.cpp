#include "enginepch.h"
#include "AssetLibrary.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <limits>
#include <system_error>

#include "AssetArtifactIO.h"
#include "AssetBinaryIO.h"

namespace {
	constexpr std::array<uint8_t, 8> LibraryMagic = { 'H', 'U', 'A', 'L', 'I', 'B', 'R', 'Y' };
	constexpr uint32_t MaxLibraryRecordCount = 1'000'000;
	constexpr uint32_t MaxDependencyCount = 1'000'000;

	bool IsEscapingPath(const std::filesystem::path& relativePath) {
		const auto normalized = relativePath.lexically_normal();
		if (normalized.empty() || normalized.is_absolute()) {
			return true;
		}
		for (const auto& component : normalized) {
			if (component == "..") {
				return true;
			}
		}
		return false;
	}

	bool IsSafeGuid(std::string_view guid) {
		if (guid.empty()) {
			return false;
		}
		return std::all_of(guid.begin(), guid.end(), [](unsigned char value) {
			return std::isalnum(value) != 0 || value == '-' || value == '_';
		});
	}

	bool IsSupportedAssetKind(HE::AssetKind kind) {
		return kind == HE::AssetKind::Mesh ||
			kind == HE::AssetKind::Material ||
			kind == HE::AssetKind::Texture2D ||
			kind == HE::AssetKind::Shader;
	}

	std::string_view ArtifactExtension(HE::AssetKind kind) {
		switch (kind) {
		case HE::AssetKind::Mesh:
			return ".huamesh";
		case HE::AssetKind::Material:
			return ".huamat";
		case HE::AssetKind::Texture2D:
			return ".huatex";
		case HE::AssetKind::Shader:
			return ".huashader";
		case HE::AssetKind::Unknown:
			break;
		}
		return {};
	}

	HE::ResultEnvelope MakeLibraryFailure(
		std::string operation,
		const std::filesystem::path& path,
		std::string code,
		std::string message) {
		auto result = HE::ResultEnvelope::Failure(std::move(operation), path.generic_string(), message);
		result.AddDetail({ HE::DiagnosticSeverity::Error, std::move(code), std::move(message), path.generic_string() });
		return result;
	}
}

namespace HE {
	ResultEnvelope AssetLibrary::Open(const ProjectContext& context) {
		m_Records.clear();
		m_IsOpen = false;
		if (!context.IsLoaded()) {
			return MakeLibraryFailure("asset.library.open", context.RootPath, "asset.library.project_unloaded", "Project context is not loaded");
		}

		m_RootPath = (context.RootPath / "Library").lexically_normal();
		m_ArtifactRootPath = m_RootPath / "Artifacts";
		m_CatalogPath = m_RootPath / "AssetLibrary.bin";

		std::error_code errorCode;
		std::filesystem::create_directories(m_ArtifactRootPath, errorCode);
		if (errorCode) {
			return MakeLibraryFailure("asset.library.open", m_RootPath, "asset.library.directory_create_failed", errorCode.message());
		}
		m_IsOpen = true;

		if (!std::filesystem::is_regular_file(m_CatalogPath, errorCode)) {
			return ResultEnvelope::Success("asset.library.open", m_RootPath.generic_string(), "Empty asset library opened");
		}

		std::string loadError;
		if (LoadCatalog(loadError)) {
			auto result = ResultEnvelope::Success("asset.library.open", m_RootPath.generic_string(), "Asset library opened");
			result.SetPayloadValue("record_count", std::to_string(m_Records.size()));
			return result;
		}

		m_Records.clear();
		auto saveResult = Save();
		if (!saveResult.Succeeded()) {
			saveResult.Operation = "asset.library.open";
			return saveResult;
		}

		auto result = ResultEnvelope::Success("asset.library.open", m_RootPath.generic_string(), "Invalid generated catalog was rebuilt");
		result.AddDetail({ DiagnosticSeverity::Warning, "asset.library.catalog_rebuilt", loadError, m_CatalogPath.generic_string() });
		result.SetPayloadValue("record_count", "0");
		return result;
	}

	ResultEnvelope AssetLibrary::Save() const {
		if (!m_IsOpen) {
			return MakeLibraryFailure("asset.library.save", m_CatalogPath, "asset.library.not_open", "Asset library is not open");
		}

		std::vector<const AssetLibraryRecord*> records;
		records.reserve(m_Records.size());
		for (const auto& [guid, record] : m_Records) {
			(void)guid;
			records.push_back(&record);
		}
		std::sort(records.begin(), records.end(), [](const auto* left, const auto* right) {
			return left->Guid < right->Guid;
		});

		AssetBinaryWriter writer;
		writer.WriteBytes(LibraryMagic);
		writer.WriteU32(AssetLibraryFormatVersion);
		writer.WriteU32(static_cast<uint32_t>(records.size()));
		for (const auto* record : records) {
			writer.WriteString(record->Guid);
			writer.WriteU32(static_cast<uint32_t>(record->Kind));
			writer.WriteString(record->ImporterId);
			writer.WriteU32(record->ImporterVersion);
			writer.WriteU32(record->ArtifactVersion);
			writer.WriteString(record->ArtifactRelativePath.generic_string());
			writer.WriteU32(static_cast<uint32_t>(record->Dependencies.size()));
			for (const auto& dependency : record->Dependencies) {
				writer.WriteString(dependency);
			}
		}

		auto result = WriteAssetBinaryFileAtomically(m_CatalogPath, writer.GetData(), "asset.library.save");
		result.SetPayloadValue("record_count", std::to_string(records.size()));
		return result;
	}

	const AssetLibraryRecord* AssetLibrary::Find(const AssetGuid& guid) const {
		const auto it = m_Records.find(guid);
		return it != m_Records.end() ? &it->second : nullptr;
	}

	bool AssetLibrary::IsArtifactAvailable(
		const AssetGuid& guid,
		AssetKind kind,
		std::string_view importerId,
		uint32_t importerVersion,
		uint32_t artifactVersion) const {
		const auto* record = Find(guid);
		if (!record ||
			record->Kind != kind ||
			record->ImporterId != importerId ||
			record->ImporterVersion != importerVersion ||
			record->ArtifactVersion != artifactVersion ||
			!IsSafeArtifactPath(record->ArtifactRelativePath)) {
			return false;
		}

		AssetArtifact artifact;
		const auto result = ReadAssetArtifactFile(ResolveArtifactPath(record->ArtifactRelativePath), artifact);
		return result.Succeeded() && artifact.Kind == kind && artifact.ArtifactVersion == artifactVersion;
	}

	ResultEnvelope AssetLibrary::CommitArtifact(
		const AssetGuid& guid,
		std::string_view importerId,
		uint32_t importerVersion,
		const AssetArtifact& artifact) {
		if (!m_IsOpen) {
			return MakeLibraryFailure("asset.library.commit", m_RootPath, "asset.library.not_open", "Asset library is not open");
		}
		const auto extension = ArtifactExtension(artifact.Kind);
		if (!IsSafeGuid(guid) || importerId.empty() || importerVersion == 0 || artifact.ArtifactVersion == 0 || extension.empty()) {
			return MakeLibraryFailure("asset.library.commit", m_RootPath, "asset.library.commit_invalid", "Artifact commit metadata is invalid");
		}

		AssetLibraryRecord record;
		record.Guid = guid;
		record.Kind = artifact.Kind;
		record.ImporterId = std::string(importerId);
		record.ImporterVersion = importerVersion;
		record.ArtifactVersion = artifact.ArtifactVersion;
		record.ArtifactRelativePath = std::filesystem::path("Artifacts") / (guid + std::string(extension));
		record.Dependencies = artifact.Dependencies;

		const auto artifactPath = ResolveArtifactPath(record.ArtifactRelativePath);
		auto writeResult = WriteAssetArtifactFile(artifactPath, artifact);
		if (!writeResult.Succeeded()) {
			writeResult.Operation = "asset.library.commit";
			return writeResult;
		}

		m_Records[guid] = std::move(record);
		auto result = ResultEnvelope::Success("asset.library.commit", guid, "Artifact committed");
		result.SetPayloadValue("artifact_path", artifactPath.generic_string());
		return result;
	}

	ResultEnvelope AssetLibrary::ReadArtifact(
		const AssetGuid& guid,
		AssetArtifact& outArtifact) const {
		const auto* record = Find(guid);
		if (!record || !IsSafeArtifactPath(record->ArtifactRelativePath)) {
			return MakeLibraryFailure("asset.library.read", m_RootPath, "asset.library.record_missing", "Asset library record is missing or invalid");
		}

		auto result = ReadAssetArtifactFile(ResolveArtifactPath(record->ArtifactRelativePath), outArtifact);
		result.Operation = "asset.library.read";
		if (result.Succeeded() &&
			(outArtifact.Kind != record->Kind || outArtifact.ArtifactVersion != record->ArtifactVersion)) {
			return MakeLibraryFailure("asset.library.read", ResolveArtifactPath(record->ArtifactRelativePath), "asset.library.artifact_mismatch", "Artifact header does not match its library record");
		}
		outArtifact.Dependencies = record->Dependencies;
		return result;
	}

	bool AssetLibrary::LoadCatalog(std::string& outError) {
		std::vector<uint8_t> bytes;
		const auto readResult = ReadAssetBinaryFile(m_CatalogPath, bytes, "asset.library.catalog_read");
		if (!readResult.Succeeded()) {
			outError = readResult.Summary;
			return false;
		}

		AssetBinaryReader reader(bytes);
		std::vector<uint8_t> magic;
		uint32_t version = 0;
		uint32_t recordCount = 0;
		if (!reader.ReadBytes(LibraryMagic.size(), magic) ||
			!std::equal(magic.begin(), magic.end(), LibraryMagic.begin(), LibraryMagic.end()) ||
			!reader.ReadU32(version) || version != AssetLibraryFormatVersion ||
			!reader.ReadU32(recordCount) || recordCount > MaxLibraryRecordCount) {
			outError = "Asset library catalog header is invalid or unsupported";
			return false;
		}

		std::unordered_map<AssetGuid, AssetLibraryRecord> loadedRecords;
		loadedRecords.reserve(recordCount);
		for (uint32_t index = 0; index < recordCount; ++index) {
			AssetLibraryRecord record;
			uint32_t kindValue = 0;
			std::string artifactPath;
			uint32_t dependencyCount = 0;
			if (!reader.ReadString(record.Guid) ||
				!reader.ReadU32(kindValue) ||
				!reader.ReadString(record.ImporterId) ||
				!reader.ReadU32(record.ImporterVersion) ||
				!reader.ReadU32(record.ArtifactVersion) ||
				!reader.ReadString(artifactPath) ||
				!reader.ReadU32(dependencyCount) ||
				dependencyCount > MaxDependencyCount) {
				outError = "Asset library catalog record is invalid or truncated";
				return false;
			}

			record.Kind = static_cast<AssetKind>(kindValue);
			record.ArtifactRelativePath = std::filesystem::path(artifactPath).lexically_normal();
			if (!IsSafeGuid(record.Guid) ||
				!IsSupportedAssetKind(record.Kind) ||
				record.ImporterId.empty() ||
				record.ImporterVersion == 0 ||
				record.ArtifactVersion == 0 ||
				!IsSafeArtifactPath(record.ArtifactRelativePath)) {
				outError = "Asset library catalog record metadata is invalid";
				return false;
			}

			record.Dependencies.reserve(dependencyCount);
			for (uint32_t dependencyIndex = 0; dependencyIndex < dependencyCount; ++dependencyIndex) {
				AssetGuid dependency;
				if (!reader.ReadString(dependency) || dependency.empty()) {
					outError = "Asset library dependency record is invalid";
					return false;
				}
				record.Dependencies.push_back(std::move(dependency));
			}

			if (!loadedRecords.emplace(record.Guid, std::move(record)).second) {
				outError = "Asset library catalog contains duplicate GUID records";
				return false;
			}
		}

		if (reader.Failed() || reader.Remaining() != 0) {
			outError = "Asset library catalog contains trailing or malformed data";
			return false;
		}

		m_Records = std::move(loadedRecords);
		return true;
	}

	bool AssetLibrary::IsSafeArtifactPath(const std::filesystem::path& relativePath) const {
		if (IsEscapingPath(relativePath)) {
			return false;
		}
		const auto normalized = relativePath.lexically_normal();
		const auto iterator = normalized.begin();
		return iterator != normalized.end() && *iterator == "Artifacts";
	}

	std::filesystem::path AssetLibrary::ResolveArtifactPath(const std::filesystem::path& relativePath) const {
		return (m_RootPath / relativePath).lexically_normal();
	}
}
