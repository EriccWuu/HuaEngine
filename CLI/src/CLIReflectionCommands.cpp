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
			const auto configArgument = options.GetValue("--meta-config");
			if (!configArgument.has_value()) {
				outError = MakeUsageError(command, "Run CMake configure first, then supply the module/configuration meta configuration", "--meta-config");
				return false;
			}
			const auto configPath = std::filesystem::u8path(*configArgument);
			outRequest.MetaConfigPath = NormalizePath(configPath.is_absolute() ? configPath : context.WorkingDirectory / configPath);

			const auto outputDirectoryArgument = options.GetValue("--out-dir");
			if (requireOutputDirectory && !outputDirectoryArgument.has_value()) {
				outError = MakeUsageError(command, "Option is required", "--out-dir");
				return false;
			}

			if (const auto rootArgument = options.GetValue("--root"); rootArgument.has_value()) {
				const auto rootPath = std::filesystem::u8path(*rootArgument);
				outRequest.RootPath = NormalizePath(rootPath.is_absolute() ? rootPath : context.WorkingDirectory / rootPath);
			}
			if (const auto entryArgument = options.GetValue("--entry-header"); entryArgument.has_value()) {
				const auto entryPath = std::filesystem::u8path(*entryArgument);
				outRequest.EntryHeader = NormalizePath(entryPath.is_absolute() ? entryPath : context.WorkingDirectory / entryPath);
			}

			if (const auto manifestArgument = options.GetValue("--out"); manifestArgument.has_value()) {
				const auto manifestPath = std::filesystem::u8path(*manifestArgument);
				outRequest.ManifestPath = NormalizePath(manifestPath.is_absolute()
					? manifestPath
					: context.WorkingDirectory / manifestPath);
			}

			if (outputDirectoryArgument.has_value()) {
				const auto outputDirectory = std::filesystem::u8path(*outputDirectoryArgument);
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
