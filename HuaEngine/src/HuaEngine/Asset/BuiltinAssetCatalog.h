#pragma once

#include <filesystem>

#include "AssetManifest.h"
#include "HuaEngine/Core/ResultEnvelope.h"

namespace HE {
	[[nodiscard]] std::filesystem::path GetBuiltinAssetRootPath();

	[[nodiscard]] ResultEnvelope LoadBuiltinAssetCatalog(
		const std::filesystem::path& builtinRoot,
		AssetManifest& outCatalog);

	[[nodiscard]] ResultEnvelope LoadBuiltinAssetCatalog(AssetManifest& outCatalog);

	[[nodiscard]] ResultEnvelope MergeBuiltinAssetCatalog(
		const AssetManifest& catalog,
		AssetManifest& inOutManifest);
}
