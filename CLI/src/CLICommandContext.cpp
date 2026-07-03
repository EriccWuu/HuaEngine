#include "enginepch.h"
#include "CLICommandContext.h"

#include <system_error>

namespace HE::CLI {
	std::filesystem::path NormalizePath(const std::filesystem::path& path) {
		if (path.empty()) {
			return {};
		}

		std::error_code errorCode;
		auto absolutePath = std::filesystem::absolute(path, errorCode);
		if (errorCode) {
			return path.lexically_normal();
		}

		if (std::filesystem::exists(absolutePath, errorCode)) {
			auto canonicalPath = std::filesystem::weakly_canonical(absolutePath, errorCode);
			if (!errorCode) {
				return canonicalPath;
			}
		}

		return absolutePath.lexically_normal();
	}

	ResultEnvelope MakeUsageError(const CLICommandDefinition& command, std::string_view summary, std::string_view context) {
		auto result = MakeUsageError(summary, context);
		result.AddDetail({ DiagnosticSeverity::Info, "cli.usage.command", command.Usage, {} });
		return result;
	}

	ResultEnvelope MakeUsageError(std::string_view summary, std::string_view context) {
		auto result = ResultEnvelope::Failure("cli.usage", "command_line", std::string(summary));
		result.AddDetail({ DiagnosticSeverity::Error, "cli.usage.invalid", std::string(summary), std::string(context) });
		result.AddDetail({ DiagnosticSeverity::Info, "cli.usage.example", "Supported commands include: ops list, project init, project status, scene create, scene validate, scene entity create/delete, scene component add/remove, asset register-default-mesh, asset validate, script status, script initialize, script update, script shutdown, reflection scan/generate/validate, validation run", {} });
		return result;
	}

	ResultEnvelope MakeHostFailure(std::string_view operation, std::string_view summary, std::string_view context) {
		auto result = ResultEnvelope::Failure(std::string(operation), "command_line", std::string(summary));
		if (!context.empty()) {
			result.AddDetail({ DiagnosticSeverity::Error, "cli.host.failure", std::string(summary), std::string(context) });
		}
		return result;
	}

	void MergeDetails(ResultEnvelope& destination, const ResultEnvelope& source) {
		for (const auto& detail : source.Details) {
			destination.Details.push_back(detail);
		}
	}

	void CopyPayloadIfMissing(ResultEnvelope& destination, const ResultEnvelope& source) {
		for (const auto& [key, value] : source.Payload) {
			destination.Payload.try_emplace(key, value);
		}
	}

	bool ResolveProjectContext(
		ApplicationOperations& operations,
		const std::optional<std::string>& explicitPath,
		const std::filesystem::path& workingDirectory,
		ProjectContext& outContext,
		ResultEnvelope& outError) {
		const auto basePath = NormalizePath(explicitPath.has_value() ? std::filesystem::path(*explicitPath) : workingDirectory);
		outError = operations.ResolveProjectContext(basePath, outContext);
		return outError.Succeeded();
	}

	std::filesystem::path ResolveProjectRelativePath(
		const std::filesystem::path& inputPath,
		const std::filesystem::path& rootPath) {
		if (inputPath.is_absolute()) {
			return NormalizePath(inputPath);
		}

		return NormalizePath(rootPath / inputPath);
	}

	std::filesystem::path ResolveScenePath(
		const std::string& sceneArgument,
		const std::optional<ProjectContext>& context,
		const std::filesystem::path& workingDirectory) {
		std::filesystem::path scenePath(sceneArgument);
		if (scenePath.is_absolute()) {
			return NormalizePath(scenePath);
		}

		if (context.has_value()) {
			return ResolveProjectRelativePath(scenePath, context->GetSceneRootPath());
		}

		return NormalizePath(workingDirectory / scenePath);
	}
}
