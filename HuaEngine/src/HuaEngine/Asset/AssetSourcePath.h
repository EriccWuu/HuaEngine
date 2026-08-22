#pragma once

#include <filesystem>

#include "AssetManifest.h"
#include "HuaEngine/Core/ResultEnvelope.h"
#include "HuaEngine/Project/ProjectContext.h"

namespace HE {
	[[nodiscard]] bool IsSafeAssetRelativePath(const std::filesystem::path& path);

	[[nodiscard]] ResultEnvelope ResolveAssetSourcePath(
		const ProjectContext& context,
		const AssetManifestRecord& record,
		std::filesystem::path& outSourcePath);
}
