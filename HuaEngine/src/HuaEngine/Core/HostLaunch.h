#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "HuaEngine/Core/Core.h"

namespace HE {
	class ENGINE_API HostLaunch {
	public:
		[[nodiscard]] static std::filesystem::path ResolveSiblingExecutable(const std::filesystem::path& executableName);
		[[nodiscard]] static bool Launch(const std::filesystem::path& executablePath, const std::vector<std::string>& arguments = {}, const std::filesystem::path& workingDirectory = {});
		[[nodiscard]] static bool LaunchSibling(const std::filesystem::path& executableName, const std::vector<std::string>& arguments = {}, const std::filesystem::path& workingDirectory = {});
	};
}
