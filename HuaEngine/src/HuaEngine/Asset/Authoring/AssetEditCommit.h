#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "HuaEngine/Asset/AssetTypes.h"

namespace HE {
	enum class AssetEditTarget {
		Source,
		Metadata
	};

	struct AssetEditCommit {
		AssetGuid Guid;
		AssetEditTarget Target = AssetEditTarget::Source;
		std::string ExpectedSourceHash;
		std::string ExpectedMetaHash;
		std::vector<uint8_t> SerializedContent;
	};

	enum class AssetApplyState {
		Applied,
		NoChanges,
		Conflict,
		ValidationFailed,
		SaveFailed,
		SavedButImportFailed
	};
}
