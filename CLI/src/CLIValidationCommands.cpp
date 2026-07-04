#include "enginepch.h"
#include "CLIValidationCommands.h"

#include "HuaEngine/Project/ProjectContext.h"
#include "HuaEngine/Scene/Scene.h"

namespace HE::CLI {
	CLICommandResponse RunValidationCommand(
		const CLICommandDefinition& command,
		const CLIParsedOptions& options,
		CLICommandContext& context) {
		if (command.Path != std::vector<std::string>{ "validation", "run" }) {
			return { MakeUsageError(command, "Unknown command") };
		}

		ProjectContext projectContext;
		ResultEnvelope resolveResult;
		if (!ResolveProjectContext(context.Operations, options.GetValue("--path"), context.WorkingDirectory, projectContext, resolveResult)) {
			return { std::move(resolveResult) };
		}

		ApplicationValidationRequest request;
		request.Project = &projectContext;
		request.IncludeAssets = options.HasFlag("--include-assets");

		Ref<Scene> scene;
		std::filesystem::path scenePath;
		if (const auto sceneArgument = options.GetValue("--scene"); sceneArgument.has_value()) {
			scenePath = ResolveScenePath(*sceneArgument, projectContext, context.WorkingDirectory);
			auto loadResult = context.Operations.LoadScene(scenePath, scene);
			if (!loadResult.Succeeded()) {
				return { std::move(loadResult) };
			}

			request.SceneTarget = scene.get();
		}

		auto result = context.Operations.Validate(request);
		result.SetPayloadValue("project_root", projectContext.RootPath.generic_string());
		if (scene) {
			result.SetPayloadValue("scene_path", scenePath.generic_string());
		}
		return { std::move(result) };
	}
}
