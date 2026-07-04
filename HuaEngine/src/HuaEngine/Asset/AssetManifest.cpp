#include "enginepch.h"
#include "AssetManifest.h"

#include <fstream>
#include <iomanip>
#include <random>
#include <regex>
#include <sstream>
#include <system_error>

namespace {
	std::string EscapeJsonString(std::string_view value) {
		std::string escaped;
		escaped.reserve(value.size());
		for (const char ch : value) {
			switch (ch) {
			case '\\':
				escaped += "\\\\";
				break;
			case '"':
				escaped += "\\\"";
				break;
			case '\n':
				escaped += "\\n";
				break;
			case '\r':
				escaped += "\\r";
				break;
			case '\t':
				escaped += "\\t";
				break;
			default:
				escaped += ch;
				break;
			}
		}
		return escaped;
	}

	std::string UnescapeJsonString(std::string value) {
		std::string unescaped;
		unescaped.reserve(value.size());
		for (size_t index = 0; index < value.size(); ++index) {
			if (value[index] != '\\' || index + 1 >= value.size()) {
				unescaped += value[index];
				continue;
			}

			const char escaped = value[++index];
			switch (escaped) {
			case 'n':
				unescaped += '\n';
				break;
			case 'r':
				unescaped += '\r';
				break;
			case 't':
				unescaped += '\t';
				break;
			default:
				unescaped += escaped;
				break;
			}
		}
		return unescaped;
	}

	bool TryReadStringField(const std::string& objectText, std::string_view fieldName, std::string& outValue) {
		const std::regex fieldRegex("\"" + std::string(fieldName) + "\"\\s*:\\s*\"((?:\\\\.|[^\"])*)\"");
		std::smatch match;
		if (!std::regex_search(objectText, match, fieldRegex)) {
			return false;
		}

		outValue = UnescapeJsonString(match[1].str());
		return true;
	}

	HE::AssetManifestRecord MakeBuiltinRecord(
		HE::AssetGuid guid,
		std::string assetId,
		HE::AssetKind kind,
		std::string builtinName) {
		HE::AssetManifestRecord record;
		record.Guid = std::move(guid);
		record.AssetId = std::move(assetId);
		record.Kind = kind;
		record.Source = HE::AssetSource::Builtin;
		record.BuiltinName = std::move(builtinName);
		record.ImportState = HE::AssetImportState::Builtin;
		return record;
	}
}

namespace HE {
	std::string GenerateAssetGuid() {
		static std::random_device randomDevice;
		static std::mt19937_64 generator(randomDevice());
		std::uniform_int_distribution<uint64_t> distribution;

		const uint64_t high = distribution(generator);
		const uint64_t low = distribution(generator);
		std::ostringstream stream;
		stream << std::hex << std::setfill('0')
			<< std::setw(16) << high
			<< std::setw(16) << low;
		return stream.str();
	}

	std::string_view ToString(AssetKind kind) {
		switch (kind) {
		case AssetKind::Mesh:
			return "mesh";
		case AssetKind::Material:
			return "material";
		case AssetKind::Texture2D:
			return "texture2d";
		case AssetKind::Unknown:
		default:
			return "unknown";
		}
	}

	std::string_view ToString(AssetSource source) {
		switch (source) {
		case AssetSource::File:
			return "file";
		case AssetSource::Builtin:
			return "builtin";
		case AssetSource::Unknown:
		default:
			return "unknown";
		}
	}

	std::string_view ToString(AssetImportState state) {
		switch (state) {
		case AssetImportState::Imported:
			return "imported";
		case AssetImportState::Registered:
			return "registered";
		case AssetImportState::Builtin:
			return "builtin";
		case AssetImportState::Missing:
			return "missing";
		case AssetImportState::Unknown:
		default:
			return "unknown";
		}
	}

	std::string_view ToString(BuiltinMeshPrimitive primitive) {
		switch (primitive) {
		case BuiltinMeshPrimitive::Quad:
			return "quad";
		case BuiltinMeshPrimitive::Cube:
			return "cube";
		case BuiltinMeshPrimitive::Sphere:
			return "sphere";
		}

		return "unknown";
	}

	AssetKind AssetKindFromString(std::string_view value) {
		if (value == "mesh") {
			return AssetKind::Mesh;
		}
		if (value == "material") {
			return AssetKind::Material;
		}
		if (value == "texture2d") {
			return AssetKind::Texture2D;
		}
		return AssetKind::Unknown;
	}

	AssetSource AssetSourceFromString(std::string_view value) {
		if (value == "file") {
			return AssetSource::File;
		}
		if (value == "builtin") {
			return AssetSource::Builtin;
		}
		return AssetSource::Unknown;
	}

	AssetImportState AssetImportStateFromString(std::string_view value) {
		if (value == "imported") {
			return AssetImportState::Imported;
		}
		if (value == "registered") {
			return AssetImportState::Registered;
		}
		if (value == "builtin") {
			return AssetImportState::Builtin;
		}
		if (value == "missing") {
			return AssetImportState::Missing;
		}
		return AssetImportState::Unknown;
	}

	const AssetManifestRecord* AssetManifest::FindByGuid(const AssetGuid& guid) const {
		const auto it = m_GuidIndex.find(guid);
		return it != m_GuidIndex.end() ? &m_Records[it->second] : nullptr;
	}

	AssetManifestRecord* AssetManifest::FindMutableByGuid(const AssetGuid& guid) {
		const auto it = m_GuidIndex.find(guid);
		return it != m_GuidIndex.end() ? &m_Records[it->second] : nullptr;
	}

	const AssetManifestRecord* AssetManifest::FindByAssetId(std::string_view assetId) const {
		const auto it = m_AssetIdIndex.find(std::string(assetId));
		return it != m_AssetIdIndex.end() ? &m_Records[it->second] : nullptr;
	}

	bool AssetManifest::Upsert(AssetManifestRecord record) {
		if (record.Guid.empty() || record.AssetId.empty()) {
			return false;
		}

		if (const auto guidIt = m_GuidIndex.find(record.Guid); guidIt != m_GuidIndex.end()) {
			auto& existing = m_Records[guidIt->second];
			if (existing.AssetId != record.AssetId) {
				m_AssetIdIndex.erase(existing.AssetId);
			}
			existing = std::move(record);
			m_AssetIdIndex[existing.AssetId] = guidIt->second;
			return true;
		}

		if (const auto assetIt = m_AssetIdIndex.find(record.AssetId); assetIt != m_AssetIdIndex.end()) {
			auto& existing = m_Records[assetIt->second];
			m_GuidIndex.erase(existing.Guid);
			existing = std::move(record);
			m_GuidIndex[existing.Guid] = assetIt->second;
			return true;
		}

		const size_t index = m_Records.size();
		m_GuidIndex[record.Guid] = index;
		m_AssetIdIndex[record.AssetId] = index;
		m_Records.emplace_back(std::move(record));
		return true;
	}

	std::filesystem::path GetAssetManifestPath(const ProjectContext& context) {
		return context.RootPath / ".hua" / "assets.json";
	}

	void SeedBuiltinAssets(AssetManifest& manifest) {
		(void)manifest.Upsert(MakeBuiltinRecord(BuiltinAssetGuids::QuadMesh, "builtin/mesh/quad", AssetKind::Mesh, "quad"));
		(void)manifest.Upsert(MakeBuiltinRecord(BuiltinAssetGuids::CubeMesh, "builtin/mesh/cube", AssetKind::Mesh, "cube"));
		(void)manifest.Upsert(MakeBuiltinRecord(BuiltinAssetGuids::SphereMesh, "builtin/mesh/sphere", AssetKind::Mesh, "sphere"));
		(void)manifest.Upsert(MakeBuiltinRecord(BuiltinAssetGuids::DefaultMaterial, "builtin/material/default", AssetKind::Material, "default"));
		(void)manifest.Upsert(MakeBuiltinRecord(BuiltinAssetGuids::FallbackMesh, "builtin/mesh/fallback", AssetKind::Mesh, "fallback"));
		(void)manifest.Upsert(MakeBuiltinRecord(BuiltinAssetGuids::FallbackMaterial, "builtin/material/fallback", AssetKind::Material, "fallback"));
	}

	ResultEnvelope LoadOrCreateAssetManifest(const ProjectContext& context, AssetManifest& outManifest) {
		const auto manifestPath = GetAssetManifestPath(context);
		std::error_code errorCode;
		if (std::filesystem::is_regular_file(manifestPath, errorCode)) {
			auto result = LoadAssetManifest(context, outManifest);
			if (!result.Succeeded()) {
				return result;
			}

			SeedBuiltinAssets(outManifest);
			auto saveResult = SaveAssetManifest(context, outManifest);
			if (!saveResult.Succeeded()) {
				return saveResult;
			}

			return ResultEnvelope::Success("asset.manifest.init", manifestPath.generic_string(), "Asset manifest loaded");
		}

		outManifest = AssetManifest();
		SeedBuiltinAssets(outManifest);
		auto saveResult = SaveAssetManifest(context, outManifest);
		if (!saveResult.Succeeded()) {
			return saveResult;
		}

		return ResultEnvelope::Success("asset.manifest.init", manifestPath.generic_string(), "Asset manifest created");
	}

	ResultEnvelope LoadAssetManifest(const ProjectContext& context, AssetManifest& outManifest) {
		const auto manifestPath = GetAssetManifestPath(context);
		std::ifstream stream(manifestPath, std::ios::in | std::ios::binary);
		if (!stream.good()) {
			auto result = ResultEnvelope::Failure("asset.manifest.load", manifestPath.generic_string(), "Asset manifest file could not be opened");
			result.AddDetail({ DiagnosticSeverity::Error, "asset.manifest.open_failed", "Failed to open .hua/assets.json for reading", manifestPath.generic_string() });
			return result;
		}

		const std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
		if (text.find("\"version\"") == std::string::npos || text.find("\"assets\"") == std::string::npos) {
			auto result = ResultEnvelope::Failure("asset.manifest.load", manifestPath.generic_string(), "Asset manifest has an unsupported shape");
			result.AddDetail({ DiagnosticSeverity::Error, "asset.manifest.invalid", "Expected top-level version and assets fields", manifestPath.generic_string() });
			return result;
		}

		AssetManifest loaded;
		const std::regex objectRegex("\\{[^{}]*\"guid\"[^{}]*\\}");
		for (auto it = std::sregex_iterator(text.begin(), text.end(), objectRegex); it != std::sregex_iterator(); ++it) {
			const std::string objectText = it->str();

			AssetManifestRecord record;
			std::string kind;
			std::string source;
			std::string importState;
			std::string relativePath;

			TryReadStringField(objectText, "guid", record.Guid);
			TryReadStringField(objectText, "asset_id", record.AssetId);
			TryReadStringField(objectText, "kind", kind);
			TryReadStringField(objectText, "source", source);
			TryReadStringField(objectText, "relative_path", relativePath);
			TryReadStringField(objectText, "builtin_name", record.BuiltinName);
			TryReadStringField(objectText, "import_state", importState);

			record.Kind = AssetKindFromString(kind);
			record.Source = AssetSourceFromString(source);
			record.RelativePath = std::filesystem::path(relativePath);
			record.ImportState = AssetImportStateFromString(importState);
			(void)loaded.Upsert(std::move(record));
		}

		outManifest = std::move(loaded);
		return ResultEnvelope::Success("asset.manifest.load", manifestPath.generic_string(), "Asset manifest loaded");
	}

	ResultEnvelope SaveAssetManifest(const ProjectContext& context, const AssetManifest& manifest) {
		const auto manifestPath = GetAssetManifestPath(context);
		std::error_code errorCode;
		std::filesystem::create_directories(manifestPath.parent_path(), errorCode);
		if (errorCode) {
			auto result = ResultEnvelope::Failure("asset.manifest.save", manifestPath.generic_string(), "Asset manifest directory could not be created");
			result.AddDetail({ DiagnosticSeverity::Error, "asset.manifest.directory_failed", errorCode.message(), manifestPath.parent_path().generic_string() });
			return result;
		}

		std::ofstream stream(manifestPath, std::ios::out | std::ios::binary | std::ios::trunc);
		if (!stream.good()) {
			auto result = ResultEnvelope::Failure("asset.manifest.save", manifestPath.generic_string(), "Asset manifest file could not be opened");
			result.AddDetail({ DiagnosticSeverity::Error, "asset.manifest.open_failed", "Failed to open .hua/assets.json for writing", manifestPath.generic_string() });
			return result;
		}

		stream << "{\n";
		stream << "  \"version\": 1,\n";
		stream << "  \"assets\": [\n";

		bool first = true;
		manifest.ForEachRecord([&](const AssetManifestRecord& record) {
			if (!first) {
				stream << ",\n";
			}
			first = false;

			stream << "    {\n";
			stream << "      \"guid\": \"" << EscapeJsonString(record.Guid) << "\",\n";
			stream << "      \"asset_id\": \"" << EscapeJsonString(record.AssetId) << "\",\n";
			stream << "      \"kind\": \"" << ToString(record.Kind) << "\",\n";
			stream << "      \"source\": \"" << ToString(record.Source) << "\",\n";
			stream << "      \"relative_path\": \"" << EscapeJsonString(record.RelativePath.generic_string()) << "\",\n";
			stream << "      \"builtin_name\": \"" << EscapeJsonString(record.BuiltinName) << "\",\n";
			stream << "      \"import_state\": \"" << ToString(record.ImportState) << "\"\n";
			stream << "    }";
		});

		stream << "\n";
		stream << "  ]\n";
		stream << "}\n";

		return ResultEnvelope::Success("asset.manifest.save", manifestPath.generic_string(), "Asset manifest saved");
	}
}
