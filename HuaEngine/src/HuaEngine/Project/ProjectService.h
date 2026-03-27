#pragma once

#include <filesystem>
#include <string_view>

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Core/ResultEnvelope.h"
#include "HuaEngine/Project/ProjectContext.h"

namespace HE {
	class ENGINE_API ProjectService {
	public:
		static constexpr std::string_view ProjectDirectoryName = ".huaengine";
		static constexpr std::string_view ProjectFileName = "project.json";

		[[nodiscard]] ResultEnvelope InitializeProject(
			const std::filesystem::path& rootPath,
			ProjectContext* outContext = nullptr,
			std::string_view projectName = {}) const;

		[[nodiscard]] ResultEnvelope ResolveProjectContext(
			const std::filesystem::path& startingPath,
			ProjectContext& outContext) const;

		[[nodiscard]] ResultEnvelope CheckProjectStatus(
			const ProjectContext& context,
			ProjectStatusReport* outReport = nullptr) const;

		[[nodiscard]] static std::filesystem::path GetMetadataDirectoryPath(const std::filesystem::path& rootPath);
		[[nodiscard]] static std::filesystem::path GetProjectFilePath(const std::filesystem::path& rootPath);
	};
}
