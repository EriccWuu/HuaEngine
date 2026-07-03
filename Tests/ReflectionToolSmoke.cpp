#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace {
	struct ProcessResult {
		DWORD ExitCode = 0;
		std::string Command;
		std::string Output;
	};

	void Expect(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[ReflectionToolSmoke] " << message << std::endl;
			std::exit(1);
		}
	}

	std::wstring Utf8ToWide(std::string_view value) {
		if (value.empty()) {
			return {};
		}

		const int required = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
		Expect(required > 0, "Failed to convert UTF-8 text to UTF-16");

		std::wstring wide(required, L'\0');
		const int written = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), wide.data(), required);
		Expect(written == required, "Failed to complete UTF-8 to UTF-16 conversion");
		return wide;
	}

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

	std::wstring QuoteForCommandLine(const std::wstring& argument) {
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

	std::wstring BuildCommandLine(const std::vector<std::string>& arguments) {
		std::wstring commandLine;
		for (const auto& argument : arguments) {
			if (!commandLine.empty()) {
				commandLine += L" ";
			}
			commandLine += QuoteForCommandLine(Utf8ToWide(argument));
		}

		return commandLine;
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

	ProcessResult RunCommand(
		const std::vector<std::string>& arguments,
		const std::filesystem::path& workingDirectory) {
		SECURITY_ATTRIBUTES securityAttributes{};
		securityAttributes.nLength = sizeof(securityAttributes);
		securityAttributes.bInheritHandle = TRUE;

		HANDLE readPipe = nullptr;
		HANDLE writePipe = nullptr;
		Expect(CreatePipe(&readPipe, &writePipe, &securityAttributes, 0), "Failed to create output pipe");
		Expect(SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0), "Failed to mark read pipe as non-inheritable");

		STARTUPINFOW startupInfo{};
		startupInfo.cb = sizeof(startupInfo);
		startupInfo.dwFlags = STARTF_USESTDHANDLES;
		startupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
		startupInfo.hStdOutput = writePipe;
		startupInfo.hStdError = writePipe;

		PROCESS_INFORMATION processInfo{};
		std::wstring commandLine = BuildCommandLine(arguments);
		std::wstring workingDirectoryWide = workingDirectory.wstring();

		const BOOL created = CreateProcessW(
			nullptr,
			commandLine.data(),
			nullptr,
			nullptr,
			TRUE,
			0,
			nullptr,
			workingDirectoryWide.c_str(),
			&startupInfo,
			&processInfo);
		CloseHandle(writePipe);
		Expect(created == TRUE, "Failed to launch command: " + BuildDisplayCommand(arguments));

		std::string output;
		char buffer[4096];
		DWORD bytesRead = 0;
		while (ReadFile(readPipe, buffer, static_cast<DWORD>(sizeof(buffer)), &bytesRead, nullptr) && bytesRead > 0) {
			output.append(buffer, bytesRead);
		}

		CloseHandle(readPipe);
		WaitForSingleObject(processInfo.hProcess, INFINITE);

		DWORD exitCode = 0;
		Expect(GetExitCodeProcess(processInfo.hProcess, &exitCode) == TRUE, "Failed to query process exit code");
		CloseHandle(processInfo.hThread);
		CloseHandle(processInfo.hProcess);

		return { exitCode, BuildDisplayCommand(arguments), std::move(output) };
	}

	void ExpectCommandSucceeded(const ProcessResult& result, const std::string& label) {
		Expect(
			result.ExitCode == 0,
			label + " should exit with code 0\nCommand: " + result.Command + "\nExit code: " +
				std::to_string(result.ExitCode) + "\nOutput:\n" + result.Output);
	}

	std::filesystem::path GetCurrentExecutablePath() {
		std::wstring buffer(MAX_PATH, L'\0');
		for (;;) {
			const DWORD required = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
			Expect(required != 0, "Failed to resolve current executable path");

			if (required < buffer.size() - 1) {
				buffer.resize(required);
				return std::filesystem::path(buffer);
			}

			buffer.resize(buffer.size() * 2);
		}
	}

	std::filesystem::path FindRepositoryRoot(std::filesystem::path start) {
		std::error_code errorCode;
		start = std::filesystem::absolute(std::move(start), errorCode);
		Expect(!errorCode, "Failed to resolve current directory");

		for (std::filesystem::path candidate = start; !candidate.empty(); candidate = candidate.parent_path()) {
			if (std::filesystem::exists(candidate / "Tools" / "Reflection" / "reflection_tool.py")) {
				return candidate;
			}

			if (candidate == candidate.parent_path()) {
				break;
			}
		}

		Expect(false, "Failed to locate repository root");
		return {};
	}
}

int main() {
	const auto binaryDirectory = GetCurrentExecutablePath().parent_path();
	const auto repositoryRoot = FindRepositoryRoot(binaryDirectory);
	const auto reflectionToolPath = repositoryRoot / "Tools" / "Reflection" / "reflection_tool.py";

	std::error_code errorCode;
	const auto workspaceReflectionDirectory = repositoryRoot / ".workspace" / "reflection";
	std::filesystem::create_directories(workspaceReflectionDirectory, errorCode);
	Expect(!errorCode, "Failed to prepare .workspace/reflection directory");

	const auto manifestPath = workspaceReflectionDirectory / "reflection_tool_smoke_manifest.json";
	const auto validatePath = workspaceReflectionDirectory / "reflection_tool_validate.json";
	std::filesystem::remove(manifestPath, errorCode);
	std::filesystem::remove(validatePath, errorCode);

	const auto scanResult = RunCommand(
		{ "python", reflectionToolPath.string(), "scan", "--root", repositoryRoot.string(), "--out", manifestPath.string() },
		repositoryRoot);
	ExpectCommandSucceeded(scanResult, "reflection tool scan");
	Expect(std::filesystem::exists(manifestPath), "reflection tool scan should write the smoke manifest");

	const auto validateResult = RunCommand(
		{ "python", reflectionToolPath.string(), "validate", "--root", repositoryRoot.string() },
		repositoryRoot);
	ExpectCommandSucceeded(validateResult, "reflection tool validate");

	std::ofstream validateOutput(validatePath, std::ios::binary);
	Expect(validateOutput.is_open(), "Failed to open reflection tool validate output file");
	validateOutput << validateResult.Output;
	validateOutput.close();
	Expect(validateOutput.good(), "Failed to write reflection tool validate output file");

	std::cout << "ReflectionToolSmoke passed" << std::endl;
	return 0;
}
