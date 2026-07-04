#include <filesystem>
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

	struct SmokeStep {
		std::string Name;
		std::vector<std::string> Arguments;
		std::vector<std::string> ExpectedFragments;
		std::vector<std::filesystem::path> ExpectedArtifacts;
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
		Expect(output.find(fragment) != std::string_view::npos, std::string(context) + ": missing fragment " + std::string(fragment));
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

	const auto tempRoot = std::filesystem::temp_directory_path() / "huaengine_cli_workflow_smoke";
	std::error_code errorCode;
	std::filesystem::remove_all(tempRoot, errorCode);
	std::filesystem::create_directories(tempRoot, errorCode);
	Expect(!errorCode, "Failed to prepare temporary workflow directory");

	const std::filesystem::path scenePath = tempRoot / "Scenes" / "workflowscene.scene";
	const std::filesystem::path meshPath = tempRoot / "Assets" / "primitives" / "quad.mesh";
	const std::filesystem::path projectMarker = tempRoot / ".huaengine" / "project.json";

	const std::vector<SmokeStep> workflow = {
		{
			"ops list",
			{ "ops", "list" },
			{
				"\"host\":\"huaengine-cli\"",
				"\"operation\":\"cli.ops_list\"",
				"\"status\":\"success\"",
				"\"data\":{\"operations\":[",
				"\"name\":\"asset.create_builtin_mesh\"",
				"\"name\":\"scene.entity.create\"",
				"\"name\":\"scene.component.add\""
			},
			{}
		},
		{
			"project init",
			{ "project", "init", "--root", tempRoot.string(), "--name", "WorkflowSmoke" },
			{
				"\"operation\":\"project.initialize\"",
				"\"status\":\"success\"",
				"\"project_name\":\"WorkflowSmoke\""
			},
			{ projectMarker }
		},
		{
			"asset register-default-mesh",
			{ "asset", "register-default-mesh", "--project", tempRoot.string(), "--asset-id", "primitives/quad.mesh", "--primitive", "quad" },
			{
				"\"operation\":\"asset.create_builtin_mesh\"",
				"\"status\":\"success\"",
				"\"asset_id\":\"primitives/quad.mesh\""
			},
			{ meshPath }
		},
		{
			"scene create",
			{ "scene", "create", "--project", tempRoot.string(), "--name", "WorkflowScene" },
			{
				"\"operation\":\"scene.create\"",
				"\"status\":\"success\"",
				"\"scene_name\":\"WorkflowScene\""
			},
			{ scenePath }
		},
		{
			"scene entity create",
			{ "scene", "entity", "create", "--project", tempRoot.string(), "--scene", "workflowscene.scene", "--name", "CliEntity" },
			{
				"\"operation\":\"scene.entity.create\"",
				"\"status\":\"success\"",
				"\"entity_id\":\""
			},
			{ scenePath }
		},
		{
			"validation run",
			{ "validation", "run", "--path", tempRoot.string(), "--scene", "workflowscene.scene", "--include-assets" },
			{
				"\"operation\":\"validation.validate\"",
				"\"status\":\"success\"",
				"\"can_continue_automatically\":true",
				"\"requires_manual_intervention\":false",
				"\"project_root\":\""
			},
			{}
		}
	};

	for (const auto& step : workflow) {
		const auto result = RunCLICommand(cliExecutable, step.Arguments, binaryDirectory);
		Expect(result.ExitCode == 0, step.Name + " should exit with code 0\n" + result.Output);

		for (const auto& fragment : step.ExpectedFragments) {
			ExpectContains(result.Output, fragment, step.Name);
		}

		for (const auto& artifact : step.ExpectedArtifacts) {
			Expect(std::filesystem::exists(artifact), step.Name + " should create artifact: " + artifact.generic_string());
		}
	}

	const auto nestedProjectDirectory = tempRoot / "Scenes" / "Nested";
	std::filesystem::create_directories(nestedProjectDirectory, errorCode);
	Expect(!errorCode, "Failed to create nested project smoke directory");

	const auto cwdProjectStatus = RunCLICommand(cliExecutable, { "project", "status" }, nestedProjectDirectory);
	Expect(cwdProjectStatus.ExitCode == 0, "cwd project status should exit with code 0\n" + cwdProjectStatus.Output);
	ExpectContains(cwdProjectStatus.Output, "\"operation\":\"project.status\"", "cwd project status");
	ExpectContains(cwdProjectStatus.Output, "\"status\":\"success\"", "cwd project status");

	std::filesystem::remove_all(tempRoot, errorCode);
	std::cout << "CLIWorkflowSmoke passed" << std::endl;
	return 0;
}
