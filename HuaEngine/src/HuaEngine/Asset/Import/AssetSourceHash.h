#pragma once

#include <filesystem>
#include <string>

#include "HuaEngine/Core/ResultEnvelope.h"

namespace HE {
	[[nodiscard]] ResultEnvelope ComputeAssetSourceHash(
		const std::filesystem::path& sourcePath,
		std::string& outHash);
}
