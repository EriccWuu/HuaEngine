#include "enginepch.h"
#include "CLIAssetCommands.h"

#include "HuaEngine/Project/ProjectContext.h"

namespace HE::CLI {
	std::optional<BuiltinMeshPrimitive> ParseBuiltinMeshPrimitive(std::string_view primitive) {
		if (primitive == "quad") {
			return BuiltinMeshPrimitive::Quad;
		}
		if (primitive == "cube") {
			return BuiltinMeshPrimitive::Cube;
		}
		if (primitive == "sphere") {
			return BuiltinMeshPrimitive::Sphere;
		}

		return std::nullopt;
	}

	CLICommandResponse RunAssetCommand(
		const CLICommandDefinition& command,
		const CLIParsedOptions& options,
		CLICommandContext& context) {
		if (command.Path == std::vector<std::string>{ "asset", "register-default-mesh" }) {
			const auto assetId = options.GetValue("--asset-id");
			if (!assetId.has_value() || assetId->empty()) {
				return { MakeUsageError("asset register-default-mesh requires --asset-id") };
			}

			const auto primitive = options.GetValue("--primitive").value_or("quad");
			const auto meshName = options.GetValue("--name").value_or(primitive);
			const auto builtinPrimitive = ParseBuiltinMeshPrimitive(primitive);
			if (!builtinPrimitive.has_value()) {
				return { MakeUsageError("Unsupported mesh primitive", primitive) };
			}

			ProjectContext projectContext;
			ResultEnvelope resolveResult;
			if (!ResolveProjectContext(context.Operations, options.GetValue("--project"), context.WorkingDirectory, projectContext, resolveResult)) {
				return { std::move(resolveResult) };
			}

			auto result = context.Operations.CreateBuiltinMeshAsset(projectContext, *assetId, *builtinPrimitive, meshName);
			return { std::move(result) };
		}

		if (command.Path == std::vector<std::string>{ "asset", "validate" }) {
			ProjectContext projectContext;
			ResultEnvelope resolveResult;
			if (!ResolveProjectContext(context.Operations, options.GetValue("--path"), context.WorkingDirectory, projectContext, resolveResult)) {
				return { std::move(resolveResult) };
			}

			return { context.Operations.ValidateAssets(projectContext) };
		}

		return { MakeUsageError(command, "Unknown command") };
	}
}
