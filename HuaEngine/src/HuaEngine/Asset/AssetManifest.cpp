#include "enginepch.h"
#include "AssetManifest.h"

#include "BuiltinAssetCatalog.h"

#include <cctype>
#include <fstream>
#include <iomanip>
#include <map>
#include <random>
#include <sstream>
#include <system_error>
#include <unordered_set>
#include <variant>

namespace {
	struct JsonValue {
		using Object = std::map<std::string, JsonValue>;
		using Array = std::vector<JsonValue>;
		using Value = std::variant<std::nullptr_t, bool, int64_t, std::string, Object, Array>;

		Value Data = nullptr;
	};

	class JsonParser {
	public:
		explicit JsonParser(std::string_view text)
			: m_Text(text) {}

		bool Parse(JsonValue& outValue, std::string& outError) {
			SkipWhitespace();
			if (!ParseValue(outValue, outError)) {
				return false;
			}

			SkipWhitespace();
			if (m_Position != m_Text.size()) {
				outError = "Unexpected trailing characters";
				return false;
			}

			return true;
		}

	private:
		bool ParseValue(JsonValue& outValue, std::string& outError) {
			SkipWhitespace();
			if (m_Position >= m_Text.size()) {
				outError = "Unexpected end of JSON";
				return false;
			}

			const char ch = m_Text[m_Position];
			if (ch == '{') {
				return ParseObject(outValue, outError);
			}
			if (ch == '[') {
				return ParseArray(outValue, outError);
			}
			if (ch == '"') {
				std::string value;
				if (!ParseString(value, outError)) {
					return false;
				}
				outValue.Data = std::move(value);
				return true;
			}
			if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch))) {
				int64_t value = 0;
				if (!ParseInteger(value, outError)) {
					return false;
				}
				outValue.Data = value;
				return true;
			}
			if (ConsumeLiteral("true")) {
				outValue.Data = true;
				return true;
			}
			if (ConsumeLiteral("false")) {
				outValue.Data = false;
				return true;
			}
			if (ConsumeLiteral("null")) {
				outValue.Data = nullptr;
				return true;
			}

			outError = "Unexpected JSON value";
			return false;
		}

		bool ParseObject(JsonValue& outValue, std::string& outError) {
			if (!Consume('{')) {
				outError = "Expected object";
				return false;
			}

			JsonValue::Object object;
			SkipWhitespace();
			if (Consume('}')) {
				outValue.Data = std::move(object);
				return true;
			}

			while (m_Position < m_Text.size()) {
				std::string key;
				if (!ParseString(key, outError)) {
					return false;
				}

				SkipWhitespace();
				if (!Consume(':')) {
					outError = "Expected ':' after object key";
					return false;
				}

				JsonValue value;
				if (!ParseValue(value, outError)) {
					return false;
				}

				if (!object.emplace(std::move(key), std::move(value)).second) {
					outError = "Duplicate object key";
					return false;
				}

				SkipWhitespace();
				if (Consume('}')) {
					outValue.Data = std::move(object);
					return true;
				}
				if (!Consume(',')) {
					outError = "Expected ',' or '}' in object";
					return false;
				}
				SkipWhitespace();
			}

			outError = "Unterminated object";
			return false;
		}

		bool ParseArray(JsonValue& outValue, std::string& outError) {
			if (!Consume('[')) {
				outError = "Expected array";
				return false;
			}

			JsonValue::Array array;
			SkipWhitespace();
			if (Consume(']')) {
				outValue.Data = std::move(array);
				return true;
			}

			while (m_Position < m_Text.size()) {
				JsonValue value;
				if (!ParseValue(value, outError)) {
					return false;
				}
				array.emplace_back(std::move(value));

				SkipWhitespace();
				if (Consume(']')) {
					outValue.Data = std::move(array);
					return true;
				}
				if (!Consume(',')) {
					outError = "Expected ',' or ']' in array";
					return false;
				}
				SkipWhitespace();
			}

			outError = "Unterminated array";
			return false;
		}

		bool ParseString(std::string& outValue, std::string& outError) {
			if (!Consume('"')) {
				outError = "Expected string";
				return false;
			}

			std::string value;
			while (m_Position < m_Text.size()) {
				const char ch = m_Text[m_Position++];
				if (ch == '"') {
					outValue = std::move(value);
					return true;
				}
				if (static_cast<unsigned char>(ch) < 0x20) {
					outError = "Control character in string";
					return false;
				}
				if (ch != '\\') {
					value += ch;
					continue;
				}
				if (m_Position >= m_Text.size()) {
					outError = "Unterminated escape sequence";
					return false;
				}

				const char escaped = m_Text[m_Position++];
				switch (escaped) {
				case '"':
				case '\\':
				case '/':
					value += escaped;
					break;
				case 'b':
					value += '\b';
					break;
				case 'f':
					value += '\f';
					break;
				case 'n':
					value += '\n';
					break;
				case 'r':
					value += '\r';
					break;
				case 't':
					value += '\t';
					break;
				default:
					outError = "Unsupported string escape sequence";
					return false;
				}
			}

			outError = "Unterminated string";
			return false;
		}

		bool ParseInteger(int64_t& outValue, std::string& outError) {
			const size_t start = m_Position;
			if (m_Text[m_Position] == '-') {
				++m_Position;
			}
			if (m_Position >= m_Text.size() || !std::isdigit(static_cast<unsigned char>(m_Text[m_Position]))) {
				outError = "Invalid number";
				return false;
			}
			if (m_Text[m_Position] == '0') {
				++m_Position;
			}
			else {
				while (m_Position < m_Text.size() && std::isdigit(static_cast<unsigned char>(m_Text[m_Position]))) {
					++m_Position;
				}
			}
			if (m_Position < m_Text.size() && (m_Text[m_Position] == '.' || m_Text[m_Position] == 'e' || m_Text[m_Position] == 'E')) {
				outError = "Manifest only supports integer numeric fields";
				return false;
			}

			try {
				outValue = std::stoll(std::string(m_Text.substr(start, m_Position - start)));
			}
			catch (...) {
				outError = "Invalid integer value";
				return false;
			}
			return true;
		}

		void SkipWhitespace() {
			while (m_Position < m_Text.size() && std::isspace(static_cast<unsigned char>(m_Text[m_Position]))) {
				++m_Position;
			}
		}

		bool Consume(char ch) {
			if (m_Position < m_Text.size() && m_Text[m_Position] == ch) {
				++m_Position;
				return true;
			}
			return false;
		}

		bool ConsumeLiteral(std::string_view literal) {
			if (m_Text.substr(m_Position, literal.size()) == literal) {
				m_Position += literal.size();
				return true;
			}
			return false;
		}

		std::string_view m_Text;
		size_t m_Position = 0;
	};

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

	bool IsEscapingAssetRoot(const std::filesystem::path& relativePath) {
		const auto normalized = relativePath.lexically_normal().generic_string();
		return normalized == ".." || normalized.rfind("../", 0) == 0 || normalized.find("/../") != std::string::npos;
	}

	HE::ResultEnvelope MakeManifestLoadFailure(
		const std::filesystem::path& manifestPath,
		std::string summary,
		std::string detailCode,
		std::string detailContext) {
		auto result = HE::ResultEnvelope::Failure("asset.manifest.load", manifestPath.generic_string(), std::move(summary));
		result.AddDetail({ HE::DiagnosticSeverity::Error, std::move(detailCode), "Asset manifest validation failed", std::move(detailContext) });
		return result;
	}

	const JsonValue::Object* AsObject(const JsonValue& value) {
		return std::get_if<JsonValue::Object>(&value.Data);
	}

	const JsonValue::Array* AsArray(const JsonValue& value) {
		return std::get_if<JsonValue::Array>(&value.Data);
	}

	bool ReadRequiredString(
		const JsonValue::Object& object,
		std::string_view fieldName,
		std::string& outValue,
		std::string& outError) {
		const auto it = object.find(std::string(fieldName));
		if (it == object.end()) {
			outError = "Missing required field: " + std::string(fieldName);
			return false;
		}

		const auto* value = std::get_if<std::string>(&it->second.Data);
		if (!value) {
			outError = "Field must be a string: " + std::string(fieldName);
			return false;
		}

		outValue = *value;
		return true;
	}

	bool ValidateRecord(const HE::AssetManifestRecord& record, std::string& outError) {
		if (record.Guid.empty()) {
			outError = "Asset guid must not be empty";
			return false;
		}
		if (record.AssetId.empty()) {
			outError = "Asset id must not be empty";
			return false;
		}
		if (record.Kind == HE::AssetKind::Unknown) {
			outError = "Asset kind is invalid";
			return false;
		}
		if (record.Source == HE::AssetSource::Unknown) {
			outError = "Asset source is invalid";
			return false;
		}
		if (record.ImportState == HE::AssetImportState::Unknown) {
			outError = "Asset import state is invalid";
			return false;
		}
		if (record.Source == HE::AssetSource::File) {
			if (record.RelativePath.empty()) {
				outError = "File asset relative_path must not be empty";
				return false;
			}
			if (record.RelativePath.is_absolute() || IsEscapingAssetRoot(record.RelativePath)) {
				outError = "File asset relative_path escapes the asset root";
				return false;
			}
		}
		return true;
	}
}

namespace HE {
	std::string GenerateAssetGuid() {
		thread_local std::random_device randomDevice;
		thread_local std::mt19937_64 generator(randomDevice());
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

		const auto guidIt = m_GuidIndex.find(record.Guid);
		const auto assetIt = m_AssetIdIndex.find(record.AssetId);

		if (guidIt != m_GuidIndex.end() && assetIt != m_AssetIdIndex.end() && guidIt->second != assetIt->second) {
			return false;
		}
		if (guidIt != m_GuidIndex.end()) {
			const auto& existing = m_Records[guidIt->second];
			if (existing.AssetId != record.AssetId) {
				return false;
			}
			m_Records[guidIt->second] = std::move(record);
			return true;
		}
		if (assetIt != m_AssetIdIndex.end()) {
			const auto& existing = m_Records[assetIt->second];
			if (existing.Guid != record.Guid) {
				return false;
			}
			m_Records[assetIt->second] = std::move(record);
			return true;
		}

		const size_t index = m_Records.size();
		m_GuidIndex[record.Guid] = index;
		m_AssetIdIndex[record.AssetId] = index;
		m_Records.emplace_back(std::move(record));
		return true;
	}

	std::filesystem::path GetAssetManifestPath(const ProjectContext& context) {
		return context.RootPath / ".huaengine" / "assets.json";
	}

	ResultEnvelope LoadOrCreateAssetManifest(const ProjectContext& context, AssetManifest& outManifest) {
		const auto manifestPath = GetAssetManifestPath(context);
		std::error_code errorCode;
		const bool manifestExists = std::filesystem::is_regular_file(manifestPath, errorCode);
		if (manifestExists) {
			auto result = LoadAssetManifest(context, outManifest);
			if (!result.Succeeded()) {
				return result;
			}
		}
		else {
			outManifest = AssetManifest();
		}

		AssetManifest builtinCatalog;
		auto catalogResult = LoadBuiltinAssetCatalog(builtinCatalog);
		if (!catalogResult.Succeeded()) {
			catalogResult.Operation = "asset.manifest.init";
			return catalogResult;
		}
		auto mergeResult = MergeBuiltinAssetCatalog(builtinCatalog, outManifest);
		if (!mergeResult.Succeeded()) {
			mergeResult.Operation = "asset.manifest.init";
			return mergeResult;
		}

		auto saveResult = SaveAssetManifest(context, outManifest);
		if (!saveResult.Succeeded()) {
			return saveResult;
		}

		return ResultEnvelope::Success(
			"asset.manifest.init",
			manifestPath.generic_string(),
			manifestExists ? "Asset manifest loaded" : "Asset manifest created");
	}

	ResultEnvelope LoadAssetManifest(const ProjectContext& context, AssetManifest& outManifest) {
		const auto manifestPath = GetAssetManifestPath(context);
		return LoadAssetManifest(manifestPath, outManifest);
	}

	ResultEnvelope LoadAssetManifest(const std::filesystem::path& manifestPath, AssetManifest& outManifest) {
		std::ifstream stream(manifestPath, std::ios::in | std::ios::binary);
		if (!stream.good()) {
			auto result = ResultEnvelope::Failure("asset.manifest.load", manifestPath.generic_string(), "Asset manifest file could not be opened");
			result.AddDetail({ DiagnosticSeverity::Error, "asset.manifest.open_failed", "Failed to open .huaengine/assets.json for reading", manifestPath.generic_string() });
			return result;
		}

		const std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
		JsonValue root;
		std::string error;
		JsonParser parser(text);
		if (!parser.Parse(root, error)) {
			return MakeManifestLoadFailure(manifestPath, "Asset manifest contains invalid JSON", "asset.manifest.json_invalid", error);
		}

		const auto* rootObject = AsObject(root);
		if (!rootObject) {
			return MakeManifestLoadFailure(manifestPath, "Asset manifest root must be an object", "asset.manifest.root_invalid", manifestPath.generic_string());
		}

		const auto versionIt = rootObject->find("version");
		if (versionIt == rootObject->end()) {
			return MakeManifestLoadFailure(manifestPath, "Asset manifest is missing version", "asset.manifest.version_missing", manifestPath.generic_string());
		}
		const auto* version = std::get_if<int64_t>(&versionIt->second.Data);
		if (!version || *version != 1) {
			return MakeManifestLoadFailure(manifestPath, "Asset manifest version is unsupported", "asset.manifest.version_invalid", manifestPath.generic_string());
		}

		const auto assetsIt = rootObject->find("assets");
		if (assetsIt == rootObject->end()) {
			return MakeManifestLoadFailure(manifestPath, "Asset manifest is missing assets", "asset.manifest.assets_missing", manifestPath.generic_string());
		}
		const auto* assets = AsArray(assetsIt->second);
		if (!assets) {
			return MakeManifestLoadFailure(manifestPath, "Asset manifest assets must be an array", "asset.manifest.assets_invalid", manifestPath.generic_string());
		}

		AssetManifest loaded;
		std::unordered_set<AssetGuid> seenGuids;
		std::unordered_set<std::string> seenAssetIds;
		for (size_t index = 0; index < assets->size(); ++index) {
			const auto* recordObject = AsObject((*assets)[index]);
			if (!recordObject) {
				return MakeManifestLoadFailure(manifestPath, "Asset manifest record must be an object", "asset.manifest.record_invalid", std::to_string(index));
			}

			AssetManifestRecord record;
			std::string kind;
			std::string source;
			std::string importState;
			std::string relativePath;

			if (!ReadRequiredString(*recordObject, "guid", record.Guid, error) ||
				!ReadRequiredString(*recordObject, "asset_id", record.AssetId, error) ||
				!ReadRequiredString(*recordObject, "kind", kind, error) ||
				!ReadRequiredString(*recordObject, "source", source, error) ||
				!ReadRequiredString(*recordObject, "relative_path", relativePath, error) ||
				!ReadRequiredString(*recordObject, "builtin_name", record.BuiltinName, error) ||
				!ReadRequiredString(*recordObject, "import_state", importState, error)) {
				return MakeManifestLoadFailure(manifestPath, "Asset manifest record is missing a required field", "asset.manifest.record_field_invalid", error);
			}

			record.Kind = AssetKindFromString(kind);
			record.Source = AssetSourceFromString(source);
			record.RelativePath = std::filesystem::path(relativePath).lexically_normal();
			record.ImportState = AssetImportStateFromString(importState);

			if (!ValidateRecord(record, error)) {
				return MakeManifestLoadFailure(manifestPath, "Asset manifest record failed validation", "asset.manifest.record_invalid", error);
			}
			if (!seenGuids.insert(record.Guid).second) {
				return MakeManifestLoadFailure(manifestPath, "Asset manifest contains a duplicate GUID", "asset.manifest.guid_duplicate", record.Guid);
			}
			if (!seenAssetIds.insert(record.AssetId).second) {
				return MakeManifestLoadFailure(manifestPath, "Asset manifest contains a duplicate asset id", "asset.manifest.asset_id_duplicate", record.AssetId);
			}
			if (!loaded.Upsert(std::move(record))) {
				return MakeManifestLoadFailure(manifestPath, "Asset manifest contains duplicate or conflicting records", "asset.manifest.record_conflict", std::to_string(index));
			}
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
			result.AddDetail({ DiagnosticSeverity::Error, "asset.manifest.open_failed", "Failed to open .huaengine/assets.json for writing", manifestPath.generic_string() });
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
