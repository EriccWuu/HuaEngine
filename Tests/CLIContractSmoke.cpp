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
		std::string Output;
	};

	struct ContractCase {
		std::string Name;
		std::vector<std::string> Arguments;
		DWORD ExpectedExitCode = 0;
		std::vector<std::string> ExpectedFragments;
	};

	void Expect(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << message << std::endl;
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
		Expect(output.find(fragment) != std::string_view::npos, std::string(context) + ": missing fragment " + std::string(fragment) + "\n" + std::string(output));
	}

	void ExpectContractCase(
		const std::filesystem::path& cliExecutable,
		const std::filesystem::path& workingDirectory,
		const ContractCase& contractCase) {
		const auto result = RunCLICommand(cliExecutable, contractCase.Arguments, workingDirectory);
		Expect(result.ExitCode == contractCase.ExpectedExitCode,
			contractCase.Name + " should exit with code " + std::to_string(contractCase.ExpectedExitCode) +
				", got " + std::to_string(result.ExitCode) + "\n" + result.Output);

		for (const auto& fragment : contractCase.ExpectedFragments) {
			ExpectContains(result.Output, fragment, contractCase.Name);
		}
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
}

int main() {
	const auto binaryDirectory = GetCurrentExecutablePath().parent_path();
	const auto cliExecutable = binaryDirectory / "HuaEngineCLI.exe";
	Expect(std::filesystem::exists(cliExecutable), "HuaEngineCLI.exe must exist next to the smoke executable");

	const auto tempRoot = std::filesystem::temp_directory_path() / "huaengine_cli_contract_smoke";
	std::error_code errorCode;
	std::filesystem::remove_all(tempRoot, errorCode);
	std::filesystem::create_directories(tempRoot, errorCode);
	Expect(!errorCode, "Failed to prepare temporary contract directory");

	const std::vector<ContractCase> failureCases = {
		{
			"empty arguments",
			{},
			1,
			{
				"\"host\":\"huaengine-cli\"",
				"\"result\":{",
				"\"operation\":\"cli.usage\"",
				"\"status\":\"failure\"",
				"\"payload\":{}",
				"\"details\":["
			}
		},
		{
			"unknown command",
			{ "definitely-unknown" },
			1,
			{ "\"operation\":\"cli.usage\"", "\"status\":\"failure\"", "\"summary\":\"Unknown command\"" }
		},
		{
			"unknown option",
			{ "project", "status", "--definitely-unknown" },
			1,
			{ "\"operation\":\"cli.usage\"", "\"status\":\"failure\"", "\"summary\":\"Unknown option\"" }
		},
		{
			"missing option value",
			{ "project", "status", "--path" },
			1,
			{ "\"operation\":\"cli.usage\"", "\"status\":\"failure\"", "\"summary\":\"Option requires a value\"" }
		},
		{
			"missing required option",
			{ "scene", "create", "--project", tempRoot.string() },
			1,
			{ "\"operation\":\"cli.usage\"", "\"status\":\"failure\"", "\"summary\":\"scene create requires --name\"" }
		},
		{
			"validation include scripts missing scene",
			{ "validation", "run", "--path", tempRoot.string(), "--include-scripts" },
			1,
			{ "\"operation\":\"cli.usage\"", "\"status\":\"failure\"", "\"summary\":\"validation run with --include-scripts requires --scene\"" }
		}
	};

	for (const auto& contractCase : failureCases) {
		ExpectContractCase(cliExecutable, binaryDirectory, contractCase);
	}

	const auto projectRoot = tempRoot / "Project";
	const auto nestedProjectDirectory = projectRoot / "Nested" / "Child";
	ExpectContractCase(cliExecutable, binaryDirectory, {
		"project init",
		{ "project", "init", "--root", projectRoot.string(), "--name", "ContractSmoke" },
		0,
		{ "\"operation\":\"project.initialize\"", "\"status\":\"success\"" }
	});
	std::filesystem::create_directories(nestedProjectDirectory, errorCode);
	Expect(!errorCode, "Failed to prepare nested project directory");
	ExpectContractCase(cliExecutable, nestedProjectDirectory, {
		"cwd project resolve",
		{ "project", "status" },
		0,
		{ "\"operation\":\"project.status\"", "\"status\":\"success\"" }
	});

	const auto brokenRoot = tempRoot / "BrokenProject";
	const auto brokenMetadataDirectory = brokenRoot / ".huaengine";
	std::filesystem::create_directories(brokenMetadataDirectory, errorCode);
	Expect(!errorCode, "Failed to prepare broken project metadata directory");
	{
		std::ofstream projectFile(brokenMetadataDirectory / "project.json", std::ios::binary);
		Expect(projectFile.good(), "Failed to create broken project marker");
		projectFile << "{ broken json";
	}

	ExpectContractCase(cliExecutable, binaryDirectory, {
		"manual intervention",
		{ "project", "status", "--path", brokenRoot.string() },
		2,
		{ "\"status\":\"manual_intervention_required\"" }
	});

	std::filesystem::remove_all(tempRoot, errorCode);
	std::cout << "CLIContractSmoke passed" << std::endl;
	return 0;
}
