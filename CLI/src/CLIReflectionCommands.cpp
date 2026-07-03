#include "enginepch.h"
#include "CLIReflectionCommands.h"

#include "HuaEngine/Reflection/ReflectionToolService.h"

namespace HE::CLI {
	namespace {
		bool BuildReflectionRequest(
			const CLICommandDefinition& command,
			const CLIParsedOptions& options,
			const CLICommandContext& context,
			bool requireOutputDirectory,
			ReflectionToolRequest& outRequest,
			ResultEnvelope& outError) {
			const auto rootArgument = options.GetValue("--root");
			if (!rootArgument.has_value()) {
				outError = MakeUsageError(command, "Option is required", "--root");
				return false;
			}

			const auto outputDirectoryArgument = options.GetValue("--out-dir");
			if (requireOutputDirectory && !outputDirectoryArgument.has_value()) {
				outError = MakeUsageError(command, "Option is required", "--out-dir");
				return false;
			}

			outRequest.RootPath = NormalizePath(std::filesystem::path(*rootArgument).is_absolute()
				? std::filesystem::path(*rootArgument)
				: context.WorkingDirectory / *rootArgument);

			if (const auto manifestArgument = options.GetValue("--out"); manifestArgument.has_value()) {
				const std::filesystem::path manifestPath(*manifestArgument);
				outRequest.ManifestPath = NormalizePath(manifestPath.is_absolute()
					? manifestPath
					: context.WorkingDirectory / manifestPath);
			}

			if (outputDirectoryArgument.has_value()) {
				const std::filesystem::path outputDirectory(*outputDirectoryArgument);
				outRequest.OutputDirectory = NormalizePath(outputDirectory.is_absolute()
					? outputDirectory
					: context.WorkingDirectory / outputDirectory);
			}

			return true;
		}
	}

	CLICommandResponse RunReflectionCommand(
		const CLICommandDefinition& command,
		const CLIParsedOptions& options,
		CLICommandContext& context) {
		ReflectionToolRequest request;
		ResultEnvelope requestError;

		if (command.Path == std::vector<std::string>{ "reflection", "scan" }) {
			if (!BuildReflectionRequest(command, options, context, false, request, requestError)) {
				return { std::move(requestError) };
			}

			return { context.Operations.ScanReflection(request) };
		}

		if (command.Path == std::vector<std::string>{ "reflection", "generate" }) {
			if (!BuildReflectionRequest(command, options, context, false, request, requestError)) {
				return { std::move(requestError) };
			}

			return { context.Operations.GenerateReflection(request) };
		}

		if (command.Path == std::vector<std::string>{ "reflection", "validate" }) {
			if (!BuildReflectionRequest(command, options, context, false, request, requestError)) {
				return { std::move(requestError) };
			}

			return { context.Operations.ValidateReflection(request) };
		}

		return { MakeUsageError(command, "Unknown command") };
	}
}
