#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>

#include "HuaEngine/Asset/AssetTypes.h"
#include "HuaEngine/Core/ResultEnvelope.h"

namespace HE {
	inline constexpr uint32_t AssetMetaVersion = 1;

	struct AssetMetaSettingsNode {
		std::map<std::string, std::string, std::less<>> Values;

		[[nodiscard]] bool operator==(const AssetMetaSettingsNode&) const = default;
	};

	struct AssetMeta {
		uint32_t MetaVersion = AssetMetaVersion;
		AssetGuid Guid;
		std::string ImporterId;
		uint32_t SettingsVersion = 1;
		AssetMetaSettingsNode Settings;

		[[nodiscard]] bool operator==(const AssetMeta&) const = default;
	};

	[[nodiscard]] std::filesystem::path GetAssetMetaPath(const std::filesystem::path& sourcePath);
	[[nodiscard]] ResultEnvelope ValidateAssetMeta(const AssetMeta& meta);
	[[nodiscard]] ResultEnvelope EncodeAssetMeta(const AssetMeta& meta, std::string& outText);
	[[nodiscard]] ResultEnvelope DecodeAssetMeta(std::string_view text, AssetMeta& outMeta);
	[[nodiscard]] ResultEnvelope LoadAssetMeta(const std::filesystem::path& sourcePath, AssetMeta& outMeta);
	[[nodiscard]] ResultEnvelope SaveAssetMeta(const std::filesystem::path& sourcePath, const AssetMeta& meta);
}
