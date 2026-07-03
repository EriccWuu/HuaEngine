#include "enginepch.h"
#include "CLIScriptCommands.h"

#include <optional>

#include "HuaEngine/Project/ProjectContext.h"
#include "HuaEngine/Scene/Scene.h"

namespace HE::CLI {
	CLICommandResponse RunScriptCommand(
		const CLICommandDefinition& command,
		const CLIParsedOptions& options,
		CLICommandContext& context) {
		const auto sceneArgument = options.GetValue("--scene");
		if (!sceneArgument.has_value() || sceneArgument->empty()) {
			return { MakeUsageError("script command requires --scene") };
		}

		std::optional<ProjectContext> projectContext;
		if (options.GetValue("--project").has_value()) {
			ProjectContext resolvedContext;
			ResultEnvelope resolveResult;
			if (!ResolveProjectContext(context.Operations, options.GetValue("--project"), context.WorkingDirectory, resolvedContext, resolveResult)) {
				return { std::move(resolveResult) };
			}
			projectContext = resolvedContext;
		}

		const auto scenePath = ResolveScenePath(*sceneArgument, projectContext, context.WorkingDirectory);
		Ref<Scene> scene;
		auto loadResult = context.Operations.LoadScene(scenePath, scene);
		if (!loadResult.Succeeded()) {
			return { std::move(loadResult) };
		}

		const auto& subcommand = command.Path[1];
		ResultEnvelope result;
		if (subcommand == "status") {
			result = context.Operations.CheckSceneScripts(*scene);
		}
		else {
			auto attachResult = context.Operations.AttachScriptRuntime(*scene);
			if (!attachResult.Succeeded()) {
				return { std::move(attachResult) };
			}

			if (subcommand == "initialize") {
				result = context.Operations.InitializeSceneScripts(*scene);
			}
			else if (subcommand == "update") {
				result = context.Operations.UpdateSceneScripts(*scene);
			}
			else if (subcommand == "shutdown") {
				result = context.Operations.ShutdownSceneScripts(*scene);
			}
			else {
				return { MakeUsageError(command, "Unknown command") };
			}
		}

		result.SetPayloadValue("scene_path", scenePath.generic_string());
		return { std::move(result) };
	}
}
