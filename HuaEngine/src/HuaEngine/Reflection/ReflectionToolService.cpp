#include "enginepch.h"
#include "ReflectionToolService.h"

#include <array>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <system_error>

#ifdef HE_PLATFORM_WINDOWS
#define HE_POPEN _popen
#define HE_PCLOSE _pclose
#else
#define HE_POPEN popen
#define HE_PCLOSE pclose
#endif

namespace {
	std::filesystem::path NormalizePath(const std::filesystem::path& path) {
		if (path.empty()) {
			return {};
		}

		std::error_code errorCode;
		auto absolutePath = std::filesystem::absolute(path, errorCode);
		if (errorCode) {
			return path.lexically_normal();
		}

		auto canonicalPath = std::filesystem::weakly_canonical(absolutePath, errorCode);
		if (!errorCode) {
			return canonicalPath;
		}

		return absolutePath.lexically_normal();
	}

	std::string QuotePath(const std::filesystem::path& path) {
		std::string value = path.string();
		std::string quoted;
		quoted.reserve(value.size() + 2);
		quoted.push_back('"');
		for (const char character : value) {
			if (character == '"') {
				quoted.push_back('\\');
			}
			quoted.push_back(character);
		}
		quoted.push_back('"');
		return quoted;
	}

	std::string ReadFileText(const std::filesystem::path& path) {
		std::ifstream stream(path, std::ios::binary);
		if (!stream) {
			return {};
		}

		std::ostringstream output;
		output << stream.rdbuf();
		return output.str();
	}

	size_t CountQualifiedNames(std::string_view text) {
		size_t count = 0;
		size_t searchFrom = 0;
		while ((searchFrom = text.find("\"qualified_name\"", searchFrom)) != std::string_view::npos) {
			++count;
			searchFrom += 16;
		}
		return count;
	}

	struct ToolExecutionResult {
		int ExitCode = -1;
		std::string Output;
	};

	ToolExecutionResult RunToolCommand(const std::string& command) {
		ToolExecutionResult result;
		std::array<char, 4096> buffer{};

		FILE* pipe = HE_POPEN(command.c_str(), "r");
		if (!pipe) {
			result.Output = "Failed to launch reflection tool process.";
			return result;
		}

		while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
			result.Output += buffer.data();
		}

		result.ExitCode = HE_PCLOSE(pipe);
		return result;
	}

	HE::ResultEnvelope MakeRequestFailure(
		std::string operation,
		const std::filesystem::path& rootPath,
		std::string summary,
		std::string context = {}) {
		auto result = HE::ResultEnvelope::Failure(std::move(operation), rootPath.generic_string(), std::move(summary));
		if (!context.empty()) {
			result.AddDetail({ HE::DiagnosticSeverity::Error, "reflection.tool.request_invalid", result.Summary, std::move(context) });
		}
		return result;
	}

	void AddCommonPayload(
		HE::ResultEnvelope& result,
		const HE::ReflectionToolRequest& request,
		std::string toolOutput,
		size_t reflectedTypeCount) {
		result.SetPayloadValue("root", request.RootPath.generic_string());
		result.SetPayloadValue("manifest", request.ManifestPath.generic_string());
		result.SetPayloadValue("tool_output", std::move(toolOutput));
		result.SetPayloadValue("reflected_type_count", std::to_string(reflectedTypeCount));
		if (!request.OutputDirectory.empty()) {
			result.SetPayloadValue("output_directory", request.OutputDirectory.generic_string());
		}
	}

	HE::ResultEnvelope RunReflectionTool(
		std::string operation,
		std::string summary,
		const HE::ReflectionToolRequest& request,
		const std::string& arguments,
		bool countFromManifest) {
		const auto toolPath = request.RootPath / "Tools" / "Reflection" / "reflection_tool.py";
		if (!std::filesystem::exists(toolPath)) {
			return MakeRequestFailure(std::move(operation), request.RootPath, "Reflection tool script was not found", toolPath.generic_string());
		}

		const std::string command = "python " + QuotePath(toolPath) + " " + arguments + " 2>&1";
		const auto execution = RunToolCommand(command);
		const std::string manifestText = countFromManifest ? ReadFileText(request.ManifestPath) : std::string();
		const size_t reflectedTypeCount = countFromManifest
			? CountQualifiedNames(manifestText)
			: CountQualifiedNames(execution.Output);

		if (execution.ExitCode != 0) {
			auto result = HE::ResultEnvelope::Failure(std::move(operation), request.RootPath.generic_string(), "Reflection tool command failed");
			AddCommonPayload(result, request, execution.Output, reflectedTypeCount);
			result.AddDetail({
				HE::DiagnosticSeverity::Error,
				"reflection.tool.exit_code",
				"Reflection tool returned a non-zero exit code",
				std::to_string(execution.ExitCode)
			});
			return result;
		}

		auto result = HE::ResultEnvelope::Success(std::move(operation), request.RootPath.generic_string(), std::move(summary));
		AddCommonPayload(result, request, execution.Output, reflectedTypeCount);
		return result;
	}
}

namespace HE {
	ReflectionToolRequest ReflectionToolService::ResolveRequestDefaults(const ReflectionToolRequest& request) const {
		ReflectionToolRequest resolved;
		resolved.RootPath = NormalizePath(request.RootPath);
		if (resolved.RootPath.empty()) {
			return resolved;
		}

		resolved.ManifestPath = request.ManifestPath.empty()
			? resolved.RootPath / ".workspace" / "reflection" / "reflection_manifest.json"
			: NormalizePath(request.ManifestPath);
		resolved.OutputDirectory = request.OutputDirectory.empty()
			? resolved.RootPath / "HuaEngine" / "src" / "HuaEngine" / "Generated"
			: NormalizePath(request.OutputDirectory);
		return resolved;
	}

	ResultEnvelope ReflectionToolService::Scan(const ReflectionToolRequest& request) const {
		const auto resolved = ResolveRequestDefaults(request);
		if (resolved.RootPath.empty()) {
			return MakeRequestFailure("reflection.scan", {}, "Reflection scan requires a root path");
		}

		return RunReflectionTool(
			"reflection.scan",
			"Reflection manifest generated",
			resolved,
			"scan --root " + QuotePath(resolved.RootPath) + " --out " + QuotePath(resolved.ManifestPath),
			true);
	}

	ResultEnvelope ReflectionToolService::Generate(const ReflectionToolRequest& request) const {
		const auto resolved = ResolveRequestDefaults(request);
		if (resolved.RootPath.empty()) {
			return MakeRequestFailure("reflection.generate", {}, "Reflection generation requires a root path");
		}

		auto scanResult = Scan(resolved);
		if (!scanResult.Succeeded()) {
			scanResult.Operation = "reflection.generate";
			scanResult.Summary = "Reflection generation failed during scan";
			return scanResult;
		}

		auto generateResult = RunReflectionTool(
			"reflection.generate",
			"Reflection files generated",
			resolved,
			"generate --manifest " + QuotePath(resolved.ManifestPath) + " --out-dir " + QuotePath(resolved.OutputDirectory),
			true);

		if (generateResult.Succeeded() && generateResult.Payload.find("tool_output") != generateResult.Payload.end()) {
			generateResult.Payload["scan_output"] = scanResult.Payload["tool_output"];
		}
		return generateResult;
	}

	ResultEnvelope ReflectionToolService::Validate(const ReflectionToolRequest& request) const {
		const auto resolved = ResolveRequestDefaults(request);
		if (resolved.RootPath.empty()) {
			return MakeRequestFailure("reflection.validate", {}, "Reflection validation requires a root path");
		}

		return RunReflectionTool(
			"reflection.validate",
			"Reflection manifest validated",
			resolved,
			"validate --root " + QuotePath(resolved.RootPath),
			false);
	}
}
