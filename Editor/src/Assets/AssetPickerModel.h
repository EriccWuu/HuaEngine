#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "HuaEngine/Asset/AssetRegistry.h"

namespace HE::Editor {
	struct AssetPickerOption {
		AssetGuid Guid;
		std::string DisplayName;
	};

	struct AssetPickerPreview {
		std::string DisplayName;
		bool Missing = false;
	};

	[[nodiscard]] std::vector<AssetPickerOption> BuildAssetPickerOptions(
		std::span<const AssetRecord> records,
		AssetKind kind);

	[[nodiscard]] AssetPickerPreview GetAssetPickerPreview(
		std::span<const AssetPickerOption> options,
		const AssetGuid& guid);

	[[nodiscard]] bool AssetPickerOptionMatches(
		const AssetPickerOption& option,
		std::string_view filter);
}
