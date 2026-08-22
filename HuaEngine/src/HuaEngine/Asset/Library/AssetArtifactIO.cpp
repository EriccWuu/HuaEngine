#include "enginepch.h"
#include "AssetArtifactIO.h"

#include <array>
#include <fstream>
#include <limits>
#include <system_error>

#include "AssetBinaryIO.h"

#ifdef HE_PLATFORM_WINDOWS
#include <Windows.h>
#endif

namespace {
	constexpr std::array<uint8_t, 8> ArtifactMagic = { 'H', 'U', 'A', 'A', 'S', 'S', 'E', 'T' };
	constexpr uint64_t MaxAssetBinaryFileBytes = 1024ull * 1024ull * 1024ull;

	bool IsSupportedAssetKind(HE::AssetKind kind) {
		return kind == HE::AssetKind::Mesh ||
			kind == HE::AssetKind::Material ||
			kind == HE::AssetKind::Texture2D;
	}

	bool ReplaceFile(const std::filesystem::path& temporaryPath, const std::filesystem::path& finalPath) {
#ifdef HE_PLATFORM_WINDOWS
		return ::MoveFileExW(
			temporaryPath.c_str(),
			finalPath.c_str(),
			MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
#else
		std::error_code errorCode;
		std::filesystem::rename(temporaryPath, finalPath, errorCode);
		return !errorCode;
#endif
	}

	HE::ResultEnvelope MakeArtifactFailure(
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
	ResultEnvelope WriteAssetBinaryFileAtomically(
		const std::filesystem::path& path,
		const std::vector<uint8_t>& bytes,
		std::string operation) {
		std::error_code errorCode;
		const auto parentPath = path.parent_path();
		if (!parentPath.empty()) {
			std::filesystem::create_directories(parentPath, errorCode);
			if (errorCode) {
				return MakeArtifactFailure(std::move(operation), path, "asset.library.directory_create_failed", errorCode.message());
			}
		}

		auto temporaryPath = path;
		temporaryPath += ".tmp";
		std::filesystem::remove(temporaryPath, errorCode);
		errorCode.clear();

		std::ofstream stream(temporaryPath, std::ios::out | std::ios::binary | std::ios::trunc);
		if (!stream.good()) {
			return MakeArtifactFailure(std::move(operation), path, "asset.library.file_open_failed", "Failed to open temporary binary file");
		}
		if (!bytes.empty()) {
			stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
		}
		stream.flush();
		if (!stream.good()) {
			stream.close();
			std::filesystem::remove(temporaryPath, errorCode);
			return MakeArtifactFailure(std::move(operation), path, "asset.library.file_write_failed", "Failed to write temporary binary file");
		}
		stream.close();

		if (!ReplaceFile(temporaryPath, path)) {
			std::filesystem::remove(temporaryPath, errorCode);
			return MakeArtifactFailure(std::move(operation), path, "asset.library.file_replace_failed", "Failed to replace binary file atomically");
		}

		return ResultEnvelope::Success(std::move(operation), path.generic_string(), "Binary file committed");
	}

	ResultEnvelope ReadAssetBinaryFile(
		const std::filesystem::path& path,
		std::vector<uint8_t>& outBytes,
		std::string operation) {
		outBytes.clear();
		std::error_code errorCode;
		const auto fileSize = std::filesystem::file_size(path, errorCode);
		if (errorCode) {
			return MakeArtifactFailure(std::move(operation), path, "asset.library.file_size_failed", errorCode.message());
		}
		if (fileSize > MaxAssetBinaryFileBytes || fileSize > static_cast<uint64_t>(std::numeric_limits<size_t>::max())) {
			return MakeArtifactFailure(std::move(operation), path, "asset.library.file_too_large", "Binary file exceeds the supported size limit");
		}

		std::ifstream stream(path, std::ios::in | std::ios::binary);
		if (!stream.good()) {
			return MakeArtifactFailure(std::move(operation), path, "asset.library.file_open_failed", "Failed to open binary file");
		}

		outBytes.resize(static_cast<size_t>(fileSize));
		if (!outBytes.empty()) {
			stream.read(reinterpret_cast<char*>(outBytes.data()), static_cast<std::streamsize>(outBytes.size()));
		}
		if (stream.gcount() != static_cast<std::streamsize>(outBytes.size())) {
			outBytes.clear();
			return MakeArtifactFailure(std::move(operation), path, "asset.library.file_read_failed", "Failed to read binary file");
		}

		return ResultEnvelope::Success(std::move(operation), path.generic_string(), "Binary file loaded");
	}

	ResultEnvelope WriteAssetArtifactFile(
		const std::filesystem::path& path,
		const AssetArtifact& artifact) {
		if (!IsSupportedAssetKind(artifact.Kind) || artifact.ArtifactVersion == 0) {
			return MakeArtifactFailure("asset.library.artifact_write", path, "asset.library.artifact_invalid", "Artifact kind and version must be valid");
		}

		AssetBinaryWriter writer;
		writer.WriteBytes(ArtifactMagic);
		writer.WriteU32(AssetArtifactContainerVersion);
		writer.WriteU32(static_cast<uint32_t>(artifact.Kind));
		writer.WriteU32(artifact.ArtifactVersion);
		writer.WriteU64(static_cast<uint64_t>(artifact.Payload.size()));
		writer.WriteBytes(artifact.Payload);
		return WriteAssetBinaryFileAtomically(path, writer.GetData(), "asset.library.artifact_write");
	}

	ResultEnvelope ReadAssetArtifactFile(
		const std::filesystem::path& path,
		AssetArtifact& outArtifact,
		AssetArtifactHeader* outHeader) {
		outArtifact = {};
		std::vector<uint8_t> bytes;
		auto readResult = ReadAssetBinaryFile(path, bytes, "asset.library.artifact_read");
		if (!readResult.Succeeded()) {
			return readResult;
		}

		AssetBinaryReader reader(bytes);
		std::vector<uint8_t> magic;
		uint32_t containerVersion = 0;
		uint32_t kindValue = 0;
		uint32_t artifactVersion = 0;
		uint64_t payloadSize = 0;
		if (!reader.ReadBytes(ArtifactMagic.size(), magic) ||
			!std::equal(magic.begin(), magic.end(), ArtifactMagic.begin(), ArtifactMagic.end()) ||
			!reader.ReadU32(containerVersion) ||
			!reader.ReadU32(kindValue) ||
			!reader.ReadU32(artifactVersion) ||
			!reader.ReadU64(payloadSize)) {
			return MakeArtifactFailure("asset.library.artifact_read", path, "asset.library.artifact_header_invalid", "Artifact header is invalid or truncated");
		}

		const auto kind = static_cast<AssetKind>(kindValue);
		if (containerVersion != AssetArtifactContainerVersion ||
			!IsSupportedAssetKind(kind) ||
			artifactVersion == 0 ||
			payloadSize != reader.Remaining() ||
			payloadSize > static_cast<uint64_t>(std::numeric_limits<size_t>::max())) {
			return MakeArtifactFailure("asset.library.artifact_read", path, "asset.library.artifact_header_invalid", "Artifact header values are unsupported or inconsistent");
		}

		outArtifact.Kind = kind;
		outArtifact.ArtifactVersion = artifactVersion;
		if (!reader.ReadBytes(static_cast<size_t>(payloadSize), outArtifact.Payload) || reader.Remaining() != 0) {
			return MakeArtifactFailure("asset.library.artifact_read", path, "asset.library.artifact_payload_invalid", "Artifact payload is invalid or truncated");
		}

		if (outHeader) {
			outHeader->ContainerVersion = containerVersion;
			outHeader->Kind = kind;
			outHeader->ArtifactVersion = artifactVersion;
			outHeader->PayloadSize = payloadSize;
		}
		return ResultEnvelope::Success("asset.library.artifact_read", path.generic_string(), "Artifact loaded");
	}
}
