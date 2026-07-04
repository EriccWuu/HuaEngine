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
	std::vector<std::filesystem::path>& CleanupPaths() {
		static std::vector<std::filesystem::path> paths;
		return paths;
	}

	void CleanupRegisteredPaths() {
		std::error_code errorCode;
		for (const auto& path : CleanupPaths()) {
			std::filesystem::remove_all(path, errorCode);
			errorCode.clear();
		}
	}

	void RegisterCleanupPath(std::filesystem::path path) {
		CleanupPaths().push_back(std::move(path));
	}

	struct ProcessResult {
		DWORD ExitCode = 0;
		std::string Output;
	};

	void Expect(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[CLIReflectionSmoke] " << message << std::endl;
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

	std::wstring BuildCommandLine(const std::filesystem::path& executable, const std::vector<std::string>& arguments) {
		std::wstring commandLine = QuoteForCommandLine(executable.wstring());
		for (const auto& argument : arguments) {
			commandLine += L" ";
			commandLine += QuoteForCommandLine(Utf8ToWide(argument));
		}

		return commandLine;
	}

	ProcessResult RunCLICommand(
		const std::filesystem::path& executable,
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
		std::wstring commandLine = BuildCommandLine(executable, arguments);
		std::wstring workingDirectoryWide = workingDirectory.wstring();

		const BOOL created = CreateProcessW(
			executable.wstring().c_str(),
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
		Expect(created == TRUE, "Failed to launch HuaEngineCLI.exe");

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

		return { exitCode, std::move(output) };
	}

	void ExpectContains(std::string_view output, std::string_view fragment, std::string_view context) {
		Expect(
			output.find(fragment) != std::string_view::npos,
			std::string(context) + ": missing fragment " + std::string(fragment) + "\n" + std::string(output));
	}

	void WriteTextFile(const std::filesystem::path& path, std::string_view content) {
		std::error_code errorCode;
		std::filesystem::create_directories(path.parent_path(), errorCode);
		Expect(!errorCode, "Failed to create fixture directory: " + path.parent_path().string());

		std::ofstream stream(path, std::ios::binary);
		Expect(stream.is_open(), "Failed to open fixture file: " + path.string());
		stream << content;
		stream.close();
		Expect(stream.good(), "Failed to write fixture file: " + path.string());
	}

	void CopyFileToFixture(const std::filesystem::path& source, const std::filesystem::path& destination) {
		std::error_code errorCode;
		std::filesystem::create_directories(destination.parent_path(), errorCode);
		Expect(!errorCode, "Failed to create fixture directory: " + destination.parent_path().string());
		std::filesystem::copy_file(source, destination, std::filesystem::copy_options::overwrite_existing, errorCode);
		Expect(!errorCode, "Failed to copy fixture file from " + source.string() + " to " + destination.string());
	}

	void ExpectResultEnvelope(
		std::string_view output,
		std::string_view operation,
		std::string_view reflectedTypeCount,
		std::string_view reflectedEnumCount,
		std::string_view context) {
		ExpectContains(output, "\"operation\":\"" + std::string(operation) + "\"", context);
		ExpectContains(output, "\"status\":\"success\"", context);
		ExpectContains(output, "\"reflected_type_count\":\"" + std::string(reflectedTypeCount) + "\"", context);
		ExpectContains(output, "\"reflected_enum_count\":\"" + std::string(reflectedEnumCount) + "\"", context);
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

	void ExpectReflectionResult(
		const std::filesystem::path& cliExecutable,
		const std::filesystem::path& workingDirectory,
		const std::filesystem::path& repositoryRoot,
		std::string_view command,
		std::string_view operation) {
		const auto result = RunCLICommand(
			cliExecutable,
			{ "reflection", std::string(command), "--root", repositoryRoot.string() },
			workingDirectory);

		Expect(result.ExitCode == 0, std::string(command) + " should exit with code 0\n" + result.Output);
		ExpectResultEnvelope(result.Output, operation, "5", "1", command);
	}

	void ExpectReflectionResult(
		const std::filesystem::path& cliExecutable,
		const std::filesystem::path& workingDirectory,
		const std::vector<std::string>& arguments,
		std::string_view operation,
		std::string_view reflectedTypeCount,
		std::string_view reflectedEnumCount,
		std::string_view context) {
		const auto result = RunCLICommand(cliExecutable, arguments, workingDirectory);

		Expect(result.ExitCode == 0, std::string(context) + " should exit with code 0\n" + result.Output);
		ExpectResultEnvelope(result.Output, operation, reflectedTypeCount, reflectedEnumCount, context);
	}

	void RunShellSafePathSmoke(
		const std::filesystem::path& cliExecutable,
		const std::filesystem::path& workingDirectory,
		const std::filesystem::path& repositoryRoot) {
		const auto fixtureRoot = repositoryRoot / ".workspace" / "reflection_cli_smoke" / "root with spaces %USERNAME% & meta";
		RegisterCleanupPath(repositoryRoot / ".workspace" / "reflection_cli_smoke");
		std::error_code errorCode;
		std::filesystem::remove_all(fixtureRoot, errorCode);
		Expect(!errorCode, "Failed to clean CLI shell-safe fixture");

		CopyFileToFixture(
			repositoryRoot / "Tools" / "Reflection" / "reflection_tool.py",
			fixtureRoot / "Tools" / "Reflection" / "reflection_tool.py");
		WriteTextFile(
			fixtureRoot / "HuaEngine" / "src" / "Fixture" / "ShellSafeComponent.h",
			"namespace HE {\n"
			"HE_REFLECT_" "COMPONENT(DisplayName=\"Shell Safe\", Category=\"Fixture\")\n"
			"struct ShellSafeComponent {\n"
			"    HE_REFLECT_" "FIELD()\n"
			"    int Value = 7;\n"
			"};\n"
			"}\n");

		const auto generatedDirectory = fixtureRoot / "HuaEngine" / "src" / "HuaEngine" / "Generated";
		ExpectReflectionResult(
			cliExecutable,
			workingDirectory,
			{ "reflection", "scan", "--root", fixtureRoot.string() },
			"reflection.scan",
			"1",
			"0",
			"reflection scan with shell-safe root");
		ExpectReflectionResult(
			cliExecutable,
			workingDirectory,
			{ "reflection", "generate", "--root", fixtureRoot.string(), "--out-dir", generatedDirectory.string() },
			"reflection.generate",
			"1",
			"0",
			"reflection generate with shell-safe root");
		ExpectReflectionResult(
			cliExecutable,
			workingDirectory,
			{ "reflection", "validate", "--root", fixtureRoot.string() },
			"reflection.validate",
			"1",
			"0",
			"reflection validate with shell-safe root");
	}
}

int main() {
	(void)CleanupPaths();
	std::atexit(CleanupRegisteredPaths);

	const auto binaryDirectory = GetCurrentExecutablePath().parent_path();
	const auto cliExecutable = binaryDirectory / "HuaEngineCLI.exe";
	Expect(std::filesystem::exists(cliExecutable), "HuaEngineCLI.exe must exist next to the smoke executable");

	const auto repositoryRoot = FindRepositoryRoot(std::filesystem::current_path());
	RegisterCleanupPath(repositoryRoot / "Tools" / "Reflection" / "__pycache__");
	RegisterCleanupPath(repositoryRoot / ".workspace" / "reflection" / "reflection_manifest.json");
	ExpectReflectionResult(cliExecutable, binaryDirectory, repositoryRoot, "scan", "reflection.scan");
	ExpectReflectionResult(cliExecutable, binaryDirectory, repositoryRoot, "validate", "reflection.validate");
	RunShellSafePathSmoke(cliExecutable, binaryDirectory, repositoryRoot);

	std::cout << "CLIReflectionSmoke passed" << std::endl;
	return 0;
}
