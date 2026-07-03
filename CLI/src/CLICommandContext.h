#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "CLICommandCatalog.h"
#include "HuaEngine/Application/ApplicationOperations.h"
#include "HuaEngine/Project/ProjectContext.h"

namespace HE::CLI {
	struct CLICommandContext {
		ApplicationOperations& Operations;
		std::filesystem::path WorkingDirectory;
		const CLICommandCatalog& Catalog;
	};

	[[nodiscard]] std::filesystem::path NormalizePath(const std::filesystem::path& path);
	[[nodiscard]] ResultEnvelope MakeUsageError(const CLICommandDefinition& command, std::string_view summary, std::string_view context = {});
	[[nodiscard]] ResultEnvelope MakeUsageError(std::string_view summary, std::string_view context = {});
	[[nodiscard]] ResultEnvelope MakeHostFailure(std::string_view operation, std::string_view summary, std::string_view context = {});

	void MergeDetails(ResultEnvelope& destination, const ResultEnvelope& source);
	void CopyPayloadIfMissing(ResultEnvelope& destination, const ResultEnvelope& source);

	[[nodiscard]] bool ResolveProjectContext(
		ApplicationOperations& operations,
		const std::optional<std::string>& explicitPath,
		const std::filesystem::path& workingDirectory,
		ProjectContext& outContext,
		ResultEnvelope& outError);

	[[nodiscard]] std::filesystem::path ResolveProjectRelativePath(
		const std::filesystem::path& inputPath,
		const std::filesystem::path& rootPath);

	[[nodiscard]] std::filesystem::path ResolveScenePath(
		const std::string& sceneArgument,
		const std::optional<ProjectContext>& context,
		const std::filesystem::path& workingDirectory);
}
