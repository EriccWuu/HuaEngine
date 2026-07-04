#include "enginepch.h"
#include "ReflectionToolService.h"

#include <fstream>
#include <sstream>
#include <string_view>
#include <system_error>
#include <vector>

#ifdef HE_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
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

	std::string_view FindJsonArraySection(std::string_view text, std::string_view name) {
		const std::string key = "\"" + std::string(name) + "\"";
		const size_t keyPosition = text.find(key);
		if (keyPosition == std::string_view::npos) {
			return {};
		}

		const size_t arrayBegin = text.find('[', keyPosition + key.size());
		if (arrayBegin == std::string_view::npos) {
			return {};
		}

		bool inString = false;
		bool escaped = false;
		size_t depth = 0;
		for (size_t index = arrayBegin; index < text.size(); ++index) {
			const char character = text[index];
			if (inString) {
				if (escaped) {
					escaped = false;
				}
				else if (character == '\\') {
					escaped = true;
				}
				else if (character == '"') {
					inString = false;
				}
				continue;
			}

			if (character == '"') {
				inString = true;
				continue;
			}
			if (character == '[') {
				++depth;
				continue;
			}
			if (character == ']') {
				--depth;
				if (depth == 0) {
					return text.substr(arrayBegin, index - arrayBegin + 1);
				}
			}
		}

		return {};
	}

	size_t CountQualifiedNamesInSection(std::string_view text, std::string_view name) {
		const std::string_view section = FindJsonArraySection(text, name);
		return section.empty() ? 0 : CountQualifiedNames(section);
	}

	struct ReflectionPayloadCounts {
		size_t ReflectedTypes = 0;
		size_t ReflectedEnums = 0;
	};

	struct ToolExecutionResult {
		int ExitCode = -1;
		std::string Output;
	};

	std::string QuoteForDisplay(std::string_view argument) {
		if (!argument.empty() && argument.find_first_of(" \t\n\v\"") == std::string_view::npos) {
			return std::string(argument);
		}

		std::string quoted = "\"";
		for (const char character : argument) {
			if (character == '"' || character == '\\') {
				quoted += '\\';
			}
			quoted += character;
		}

		quoted += "\"";
		return quoted;
	}

	std::string BuildDisplayCommand(const std::vector<std::string>& arguments) {
		std::string command;
		for (const auto& argument : arguments) {
			if (!command.empty()) {
				command += " ";
			}
			command += QuoteForDisplay(argument);
		}

		return command;
	}

#ifdef HE_PLATFORM_WINDOWS
	std::wstring Utf8ToWide(std::string_view value) {
		if (value.empty()) {
			return {};
		}

		const int required = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
		if (required <= 0) {
			return {};
		}

		std::wstring wide(required, L'\0');
		const int written = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), wide.data(), required);
		if (written != required) {
			return {};
		}

		return wide;
	}

	struct ToolArgument {
		std::wstring ProcessValue;
		std::string DisplayValue;
	};

	ToolArgument LiteralArgument(std::string_view value) {
		return { Utf8ToWide(value), std::string(value) };
	}

	ToolArgument PathArgument(const std::filesystem::path& path) {
		return { path.wstring(), path.generic_string() };
	}

	std::wstring QuoteForWindowsCommandLine(const std::wstring& argument) {
		if (argument.empty()) {
			return L"\"\"";
		}

		if (argument.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
			return argument;
		}

		std::wstring quoted = L"\"";
		size_t backslashCount = 0;
		for (const wchar_t character : argument) {
			if (character == L'\\') {
				++backslashCount;
				continue;
			}

			if (character == L'"') {
				quoted.append(backslashCount * 2 + 1, L'\\');
				quoted += L'"';
				backslashCount = 0;
				continue;
			}

			if (backslashCount > 0) {
				quoted.append(backslashCount, L'\\');
				backslashCount = 0;
			}

			quoted += character;
		}

		if (backslashCount > 0) {
			quoted.append(backslashCount * 2, L'\\');
		}

		quoted += L"\"";
		return quoted;
	}

	std::wstring BuildWindowsCommandLine(const std::vector<ToolArgument>& arguments) {
		std::wstring commandLine;
		for (const auto& argument : arguments) {
			if (!commandLine.empty()) {
				commandLine += L" ";
			}
			commandLine += QuoteForWindowsCommandLine(argument.ProcessValue);
		}

		return commandLine;
	}

	std::string BuildDisplayCommand(const std::vector<ToolArgument>& arguments) {
		std::string command;
		for (const auto& argument : arguments) {
			if (!command.empty()) {
				command += " ";
			}
			command += QuoteForDisplay(argument.DisplayValue);
		}

		return command;
	}

	struct ScopedHandle {
		HANDLE Value = nullptr;

		ScopedHandle() = default;
		explicit ScopedHandle(HANDLE handle) : Value(handle) {}
		ScopedHandle(const ScopedHandle&) = delete;
		ScopedHandle& operator=(const ScopedHandle&) = delete;

		~ScopedHandle() {
			Reset();
		}

		HANDLE Get() const {
			return Value;
		}

		HANDLE* Put() {
			Reset();
			return &Value;
		}

		void Reset(HANDLE handle = nullptr) {
			if (Value != nullptr && Value != INVALID_HANDLE_VALUE) {
				CloseHandle(Value);
			}
			Value = handle;
		}
	};

	ToolExecutionResult RunToolCommand(const std::vector<ToolArgument>& arguments) {
		ToolExecutionResult result;

		SECURITY_ATTRIBUTES securityAttributes{};
		securityAttributes.nLength = sizeof(securityAttributes);
		securityAttributes.bInheritHandle = TRUE;

		ScopedHandle readPipe;
		ScopedHandle writePipe;
		if (!CreatePipe(readPipe.Put(), writePipe.Put(), &securityAttributes, 0)) {
			result.Output = "Failed to create reflection tool output pipe.";
			return result;
		}
		if (!SetHandleInformation(readPipe.Get(), HANDLE_FLAG_INHERIT, 0)) {
			result.Output = "Failed to configure reflection tool output pipe.";
			return result;
		}

		SIZE_T attributeListSize = 0;
		InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeListSize);
		if (attributeListSize == 0) {
			result.Output = "Failed to size reflection tool process attributes.";
			return result;
		}

		std::vector<unsigned char> attributeListStorage(attributeListSize);
		auto* attributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributeListStorage.data());
		if (!InitializeProcThreadAttributeList(attributeList, 1, 0, &attributeListSize)) {
			result.Output = "Failed to initialize reflection tool process attributes.";
			return result;
		}

		HANDLE inheritedHandles[] = { writePipe.Get() };
		if (!UpdateProcThreadAttribute(
			attributeList,
			0,
			PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
			inheritedHandles,
			sizeof(inheritedHandles),
			nullptr,
			nullptr)) {
			DeleteProcThreadAttributeList(attributeList);
			result.Output = "Failed to configure reflection tool inherited handles.";
			return result;
		}

		STARTUPINFOEXW startupInfo{};
		startupInfo.StartupInfo.cb = sizeof(startupInfo);
		startupInfo.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
		startupInfo.StartupInfo.hStdInput = nullptr;
		startupInfo.StartupInfo.hStdOutput = writePipe.Get();
		startupInfo.StartupInfo.hStdError = writePipe.Get();
		startupInfo.lpAttributeList = attributeList;

		PROCESS_INFORMATION processInfo{};
		std::wstring commandLine = BuildWindowsCommandLine(arguments);
		const BOOL created = CreateProcessW(
			nullptr,
			commandLine.data(),
			nullptr,
			nullptr,
			TRUE,
			EXTENDED_STARTUPINFO_PRESENT,
			nullptr,
			nullptr,
			&startupInfo.StartupInfo,
			&processInfo);
		DeleteProcThreadAttributeList(attributeList);
		writePipe.Reset();

		if (!created) {
			result.Output = "Failed to launch reflection tool process.";
			return result;
		}

		char buffer[4096];
		DWORD bytesRead = 0;
		while (ReadFile(readPipe.Get(), buffer, static_cast<DWORD>(sizeof(buffer)), &bytesRead, nullptr) && bytesRead > 0) {
			result.Output.append(buffer, bytesRead);
		}

		readPipe.Reset();
		WaitForSingleObject(processInfo.hProcess, INFINITE);

		DWORD exitCode = 1;
		if (GetExitCodeProcess(processInfo.hProcess, &exitCode)) {
			result.ExitCode = static_cast<int>(exitCode);
		}
		CloseHandle(processInfo.hThread);
		CloseHandle(processInfo.hProcess);
		return result;
	}
#else
	using ToolArgument = std::string;

	ToolArgument LiteralArgument(std::string_view value) {
		return std::string(value);
	}

	ToolArgument PathArgument(const std::filesystem::path& path) {
		return path.string();
	}

	ToolExecutionResult RunToolCommand(const std::vector<std::string>& arguments) {
		ToolExecutionResult result;

		int outputPipe[2] = { -1, -1 };
		if (pipe(outputPipe) != 0) {
			result.Output = "Failed to create reflection tool output pipe.";
			return result;
		}

		const pid_t child = fork();
		if (child < 0) {
			close(outputPipe[0]);
			close(outputPipe[1]);
			result.Output = "Failed to fork reflection tool process.";
			return result;
		}

		if (child == 0) {
			dup2(outputPipe[1], STDOUT_FILENO);
			dup2(outputPipe[1], STDERR_FILENO);
			close(outputPipe[0]);
			close(outputPipe[1]);

			std::vector<char*> argv;
			argv.reserve(arguments.size() + 1);
			for (const auto& argument : arguments) {
				argv.push_back(const_cast<char*>(argument.c_str()));
			}
			argv.push_back(nullptr);
			execvp(argv[0], argv.data());
			_exit(127);
		}

		close(outputPipe[1]);
		char buffer[4096];
		ssize_t bytesRead = 0;
		while ((bytesRead = read(outputPipe[0], buffer, sizeof(buffer))) > 0) {
			result.Output.append(buffer, static_cast<size_t>(bytesRead));
		}
		close(outputPipe[0]);

		int status = 0;
		if (waitpid(child, &status, 0) == child) {
			if (WIFEXITED(status)) {
				result.ExitCode = WEXITSTATUS(status);
			}
			else if (WIFSIGNALED(status)) {
				result.ExitCode = 128 + WTERMSIG(status);
			}
		}
		return result;
	}
#endif

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
		ReflectionPayloadCounts counts) {
		result.SetPayloadValue("root", request.RootPath.generic_string());
		result.SetPayloadValue("manifest", request.ManifestPath.generic_string());
		result.SetPayloadValue("tool_output", std::move(toolOutput));
		result.SetPayloadValue("reflected_type_count", std::to_string(counts.ReflectedTypes));
		result.SetPayloadValue("reflected_enum_count", std::to_string(counts.ReflectedEnums));
		if (!request.OutputDirectory.empty()) {
			result.SetPayloadValue("output_directory", request.OutputDirectory.generic_string());
		}
	}

	HE::ResultEnvelope RunReflectionTool(
		std::string operation,
		std::string summary,
		const HE::ReflectionToolRequest& request,
		const std::vector<ToolArgument>& toolArguments,
		bool countFromManifest) {
		const auto toolPath = request.RootPath / "Tools" / "Reflection" / "reflection_tool.py";
		if (!std::filesystem::exists(toolPath)) {
			return MakeRequestFailure(std::move(operation), request.RootPath, "Reflection tool script was not found", toolPath.generic_string());
		}

		std::vector<ToolArgument> arguments;
		arguments.reserve(toolArguments.size() + 2);
		arguments.push_back(LiteralArgument("python"));
		arguments.push_back(PathArgument(toolPath));
		arguments.insert(arguments.end(), toolArguments.begin(), toolArguments.end());

		const auto execution = RunToolCommand(arguments);
		const std::string manifestText = countFromManifest ? ReadFileText(request.ManifestPath) : std::string();
		const std::string_view countSource = countFromManifest ? std::string_view(manifestText) : std::string_view(execution.Output);
		const ReflectionPayloadCounts counts{
			CountQualifiedNamesInSection(countSource, "types"),
			CountQualifiedNamesInSection(countSource, "enums")
		};

		if (execution.ExitCode != 0) {
			auto result = HE::ResultEnvelope::Failure(std::move(operation), request.RootPath.generic_string(), "Reflection tool command failed");
			AddCommonPayload(result, request, execution.Output, counts);
			result.AddDetail({
				HE::DiagnosticSeverity::Error,
				"reflection.tool.exit_code",
				"Reflection tool returned a non-zero exit code",
				std::to_string(execution.ExitCode) + " from " + BuildDisplayCommand(arguments)
			});
			return result;
		}

		auto result = HE::ResultEnvelope::Success(std::move(operation), request.RootPath.generic_string(), std::move(summary));
		AddCommonPayload(result, request, execution.Output, counts);
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
			{ LiteralArgument("scan"), LiteralArgument("--root"), PathArgument(resolved.RootPath), LiteralArgument("--out"), PathArgument(resolved.ManifestPath) },
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
			{ LiteralArgument("generate"), LiteralArgument("--manifest"), PathArgument(resolved.ManifestPath), LiteralArgument("--out-dir"), PathArgument(resolved.OutputDirectory) },
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
			{ LiteralArgument("validate"), LiteralArgument("--root"), PathArgument(resolved.RootPath) },
			false);
	}
}
