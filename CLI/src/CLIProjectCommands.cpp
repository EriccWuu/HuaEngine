#include "enginepch.h"
#include "CLIProjectCommands.h"

#include "HuaEngine/Project/ProjectContext.h"

namespace HE::CLI {
	CLICommandResponse RunProjectCommand(
		const CLICommandDefinition& command,
		const CLIParsedOptions& options,
		CLICommandContext& context) {
		if (command.Path == std::vector<std::string>{ "project", "init" }) {
			ProjectContext projectContext;
			auto result = context.Operations.InitializeProject(
				options.GetValue("--root").value_or(NormalizePath(context.WorkingDirectory).string()),
				&projectContext,
				options.GetValue("--name").value_or(std::string()));
			return { std::move(result) };
		}

		if (command.Path == std::vector<std::string>{ "project", "status" }) {
			ProjectContext projectContext;
			ResultEnvelope resolveResult;
			if (!ResolveProjectContext(context.Operations, options.GetValue("--path"), context.WorkingDirectory, projectContext, resolveResult)) {
				return { std::move(resolveResult) };
			}

			return { context.Operations.CheckProjectStatus(projectContext) };
		}

		return { MakeUsageError(command, "Unknown command") };
	}
}
