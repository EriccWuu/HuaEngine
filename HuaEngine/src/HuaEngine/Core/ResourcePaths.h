#pragma once

#include <filesystem>

#include "HuaEngine/Core/Core.h"

namespace HE {
	class ENGINE_API ResourcePaths {
	public:
		[[nodiscard]] static std::filesystem::path GetExecutablePath();
		[[nodiscard]] static std::filesystem::path GetExecutableDirectory();
		[[nodiscard]] static std::filesystem::path GetEngineResourceRoot();
		[[nodiscard]] static std::filesystem::path ResolveRuntimePath(const std::filesystem::path& path);
		[[nodiscard]] static std::filesystem::path ResolveEngineResourcePath(const std::filesystem::path& relativePath);
	};
}
