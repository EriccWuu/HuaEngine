#include "enginepch.h"
#include "AssetMeta.h"

#include <fstream>
#include <set>
#include <vector>

#include "HuaEngine/Asset/Library/AssetArtifactIO.h"
#include "yaml-cpp/yaml.h"

namespace {
	HE::ResultEnvelope Failure(std::string code, std::string message, std::string target = {}) {
		auto result = HE::ResultEnvelope::Failure("asset.meta", std::move(target), message);
		result.AddDetail({ HE::DiagnosticSeverity::Error, std::move(code), std::move(message), result.Target });
		return result;
	}

	bool HasUniqueKeys(const YAML::Node& node) {
		if (!node.IsMap()) return false;
		std::set<std::string> keys;
		for (const auto& entry : node) {
			if (!entry.first.IsScalar() || !keys.emplace(entry.first.Scalar()).second) return false;
		}
		return true;
	}
}

namespace HE {
	std::filesystem::path GetAssetMetaPath(const std::filesystem::path& sourcePath) {
		auto path = sourcePath;
		path += ".meta";
		return path;
	}

	ResultEnvelope ValidateAssetMeta(const AssetMeta& meta) {
		if (meta.MetaVersion != AssetMetaVersion) return Failure("asset.meta.version_unsupported", "Asset metadata version is unsupported");
		if (meta.Guid.empty()) return Failure("asset.meta.invalid", "Asset metadata GUID is empty");
		if (meta.ImporterId.empty()) return Failure("asset.meta.invalid", "Asset metadata importer is empty", meta.Guid);
		if (meta.SettingsVersion == 0) return Failure("asset.meta.invalid", "Asset metadata settings version must be positive", meta.Guid);
		for (const auto& [key, value] : meta.Settings.Values) {
			if (key.empty()) return Failure("asset.meta.invalid", "Asset metadata contains an empty settings key", meta.Guid);
		}
		return ResultEnvelope::Success("asset.meta.validate", meta.Guid, "Asset metadata is valid");
	}

	ResultEnvelope EncodeAssetMeta(const AssetMeta& meta, std::string& outText) {
		outText.clear();
		auto validation = ValidateAssetMeta(meta);
		if (!validation.Succeeded()) return validation;
		YAML::Emitter emitter;
		emitter << YAML::BeginMap;
		emitter << YAML::Key << "meta_version" << YAML::Value << meta.MetaVersion;
		emitter << YAML::Key << "guid" << YAML::Value << meta.Guid;
		emitter << YAML::Key << "importer" << YAML::Value << meta.ImporterId;
		emitter << YAML::Key << "settings_version" << YAML::Value << meta.SettingsVersion;
		emitter << YAML::Key << "settings" << YAML::Value << YAML::BeginMap;
		for (const auto& [key, value] : meta.Settings.Values) emitter << YAML::Key << key << YAML::Value << value;
		emitter << YAML::EndMap << YAML::EndMap;
		if (!emitter.good()) return Failure("asset.meta.invalid", emitter.GetLastError(), meta.Guid);
		outText.assign(emitter.c_str(), emitter.size());
		outText.push_back('\n');
		return ResultEnvelope::Success("asset.meta.encode", meta.Guid, "Asset metadata encoded");
	}

	ResultEnvelope DecodeAssetMeta(std::string_view text, AssetMeta& outMeta) {
		outMeta = {};
		try {
			const auto root = YAML::Load(std::string(text));
			if (!HasUniqueKeys(root)) return Failure("asset.meta.invalid", "Asset metadata root must be a map with unique keys");
			static const std::set<std::string> allowed = { "meta_version", "guid", "importer", "settings_version", "settings" };
			for (const auto& entry : root) if (!allowed.contains(entry.first.Scalar())) return Failure("asset.meta.invalid", "Asset metadata contains an unknown field", entry.first.Scalar());
			if (!root["meta_version"] || !root["guid"] || !root["importer"] || !root["settings_version"] || !root["settings"]) return Failure("asset.meta.invalid", "Asset metadata is missing a required field");
			outMeta.MetaVersion = root["meta_version"].as<uint32_t>();
			outMeta.Guid = root["guid"].as<std::string>();
			outMeta.ImporterId = root["importer"].as<std::string>();
			outMeta.SettingsVersion = root["settings_version"].as<uint32_t>();
			const auto settings = root["settings"];
			if (!HasUniqueKeys(settings)) return Failure("asset.meta.invalid", "Asset metadata settings must be a map with unique keys", outMeta.Guid);
			for (const auto& entry : settings) {
				if (!entry.second.IsScalar()) return Failure("asset.meta.invalid", "Asset metadata settings values must be scalar", entry.first.Scalar());
				outMeta.Settings.Values.emplace(entry.first.Scalar(), entry.second.Scalar());
			}
		}
		catch (const YAML::Exception& error) {
			return Failure("asset.meta.invalid", error.what());
		}
		return ValidateAssetMeta(outMeta);
	}

	ResultEnvelope LoadAssetMeta(const std::filesystem::path& sourcePath, AssetMeta& outMeta) {
		const auto metaPath = GetAssetMetaPath(sourcePath);
		std::ifstream stream(metaPath, std::ios::in | std::ios::binary);
		if (!stream.good()) return Failure("asset.meta.missing", "Asset metadata sidecar is missing", metaPath.generic_string());
		std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
		auto result = DecodeAssetMeta(text, outMeta);
		result.Target = metaPath.generic_string();
		return result;
	}

	ResultEnvelope SaveAssetMeta(const std::filesystem::path& sourcePath, const AssetMeta& meta) {
		std::string text;
		auto encodeResult = EncodeAssetMeta(meta, text);
		if (!encodeResult.Succeeded()) return encodeResult;
		const std::vector<uint8_t> bytes(text.begin(), text.end());
		auto result = WriteAssetBinaryFileAtomically(GetAssetMetaPath(sourcePath), bytes, "asset.meta.save");
		if (!result.Succeeded()) result.AddDetail({ DiagnosticSeverity::Error, "asset.meta.save_failed", "Asset metadata could not be committed atomically", GetAssetMetaPath(sourcePath).generic_string() });
		return result;
	}
}
