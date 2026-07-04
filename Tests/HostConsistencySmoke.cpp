#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "HuaEngine.h"
#include "Workbench/EditorWorkbenchState.h"

namespace {
	struct ProcessResult {
		DWORD ExitCode = 0;
		std::string Output;
	};

	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[HostConsistencySmoke] " << message << std::endl;
			std::exit(1);
		}
	}

	void RequireContains(const std::string& haystack, const std::string& needle, const std::string& message) {
		Require(haystack.find(needle) != std::string::npos, message);
	}

	HE::ApplicationSpecification MakeApplicationSpecification() {
		HE::ApplicationSpecification specification;
		specification.Name = "HostConsistencySmoke";
		specification.EnableGuiLayer = false;
		specification.EnableWindow = false;
		return specification;
	}

	class SmokeApplication final : public HE::Application {
	public:
		SmokeApplication()
			: HE::Application(MakeApplicationSpecification()) {}
	};

	std::wstring Utf8ToWide(std::string_view value) {
		if (value.empty()) {
			return {};
		}

		const int required = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
		Require(required > 0, "Failed to convert UTF-8 to UTF-16");
		std::wstring wide(required, L'\0');
		const int written = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), wide.data(), required);
		Require(written == required, "Failed to complete UTF-8 to UTF-16 conversion");
		return wide;
	}

	std::wstring QuoteForCommandLine(const std::wstring& argument) {
		if (argument.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
			return argument;
		}

		return L"\"" + argument + L"\"";
	}

	std::filesystem::path GetCurrentExecutableDirectory() {
		std::wstring buffer(MAX_PATH, L'\0');
		const DWORD required = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
		Require(required != 0, "Failed to resolve executable path");
		buffer.resize(required);
		return std::filesystem::path(buffer).parent_path();
	}

	ProcessResult RunProcess(
		const std::filesystem::path& executable,
		const std::vector<std::string>& arguments,
		const std::filesystem::path& workingDirectory) {
		SECURITY_ATTRIBUTES securityAttributes{};
		securityAttributes.nLength = sizeof(securityAttributes);
		securityAttributes.bInheritHandle = TRUE;

		HANDLE readPipe = nullptr;
		HANDLE writePipe = nullptr;
		Require(CreatePipe(&readPipe, &writePipe, &securityAttributes, 0), "Failed to create process pipe");
		Require(SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0), "Failed to mark process pipe");

		std::wstring commandLine = QuoteForCommandLine(executable.wstring());
		for (const auto& argument : arguments) {
			commandLine += L" ";
			commandLine += QuoteForCommandLine(Utf8ToWide(argument));
		}

		STARTUPINFOW startupInfo{};
		startupInfo.cb = sizeof(startupInfo);
		startupInfo.dwFlags = STARTF_USESTDHANDLES;
		startupInfo.hStdOutput = writePipe;
		startupInfo.hStdError = writePipe;
		startupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

		PROCESS_INFORMATION processInfo{};
		const BOOL created = CreateProcessW(
			executable.wstring().c_str(),
			commandLine.data(),
			nullptr,
			nullptr,
			TRUE,
			0,
			nullptr,
			workingDirectory.wstring().c_str(),
			&startupInfo,
			&processInfo);
		CloseHandle(writePipe);
		Require(created == TRUE, "Failed to launch cli host");

		std::string output;
		char buffer[4096];
		DWORD bytesRead = 0;
		while (ReadFile(readPipe, buffer, static_cast<DWORD>(sizeof(buffer)), &bytesRead, nullptr) && bytesRead > 0) {
			output.append(buffer, bytesRead);
		}

		CloseHandle(readPipe);
		WaitForSingleObject(processInfo.hProcess, INFINITE);
		DWORD exitCode = 0;
		Require(GetExitCodeProcess(processInfo.hProcess, &exitCode) == TRUE, "Failed to read cli exit code");
		CloseHandle(processInfo.hThread);
		CloseHandle(processInfo.hProcess);
		return { exitCode, std::move(output) };
	}
}

int main() {
	HE::Log::Init({ .EnableConsoleOutput = false });
	SmokeApplication application;
	application.Start();

	HE::AgentHostAdapter adapter(application.GetOperations());
	HE::EditorWorkbenchState workbenchState;
	const auto smokeRoot = std::filesystem::temp_directory_path() / "HuaEngineHostConsistencySmoke";
	std::error_code errorCode;
	std::filesystem::remove_all(smokeRoot, errorCode);

	auto projectInit = adapter.Invoke({
		.Operation = "project.initialize",
		.Arguments = { { "root", (smokeRoot / "Project").string() }, { "name", "ConsistencyProject" } },
		.WorkingDirectory = smokeRoot
	});
	Require(projectInit.Result.Succeeded(), "Expected project.initialize to succeed");

	auto assetCreate = adapter.Invoke({
		.Operation = "asset.create_builtin_mesh",
		.Arguments = {
			{ "project_path", (smokeRoot / "Project").string() },
			{ "asset_id", "Meshes/ConsistencyQuad.mesh" },
			{ "primitive", "quad" }
		},
		.WorkingDirectory = smokeRoot
	});
	Require(assetCreate.Result.Succeeded(), "Expected asset.create_builtin_mesh to succeed");
	Require(assetCreate.Result.Payload.find("asset_guid") != assetCreate.Result.Payload.end(), "Expected asset create payload to include asset guid");
	Require(!assetCreate.Result.Payload.at("asset_guid").empty(), "Expected asset create payload to include asset guid");
	Require(assetCreate.Result.Payload.find("asset_handle") != assetCreate.Result.Payload.end(), "Expected runtime handle payload");
	Require(assetCreate.Result.Payload.at("asset_handle") != "0", "Expected runtime handle payload");

	auto sceneCreate = adapter.Invoke({
		.Operation = "scene.create",
		.Arguments = {
			{ "project_path", (smokeRoot / "Project").string() },
			{ "scene_name", "ConsistencyScene" },
			{ "output", "consistency.scene" }
		},
		.WorkingDirectory = smokeRoot
	});
	Require(sceneCreate.Result.Succeeded(), "Expected scene.create to succeed");

	HE::ProjectContext context;
	auto resolveContext = application.GetOperations().ResolveProjectContext(smokeRoot / "Project", context);
	Require(resolveContext.Succeeded(), "Expected project.resolve_context to succeed");

	HE::Ref<HE::Scene> scene;
	auto loadScene = application.GetOperations().LoadScene(context.GetAssetRootPath() / "consistency.scene", scene);
	Require(loadScene.Succeeded() && scene, "Expected scene.load to succeed");

	HE::ValidationReport validationReport;
	HE::ApplicationValidationRequest validationRequest;
	validationRequest.Project = &context;
	validationRequest.SceneTarget = scene.get();
	validationRequest.IncludeAssets = true;

	auto guiFacingValidation = application.GetOperations().Validate(validationRequest, &validationReport);
	Require(guiFacingValidation.Succeeded(), "Expected in-process validation to succeed");
	workbenchState.CaptureValidation(guiFacingValidation, validationReport, "gui.validation");

	const auto cliExecutable = GetCurrentExecutableDirectory() / "HuaEngineCLI.exe";
	Require(std::filesystem::exists(cliExecutable), "Expected HuaEngineCLI.exe next to HostConsistencySmoke.exe");

	const auto cliValidation = RunProcess(
		cliExecutable,
		{
			"validation", "run",
			"--path", (smokeRoot / "Project").string(),
			"--scene", "consistency.scene",
			"--include-assets"
		},
		GetCurrentExecutableDirectory());

	Require(cliValidation.ExitCode == 0, "Expected cli validation to succeed");
	const auto* guiResult = workbenchState.GetLastValidationResult();
	Require(guiResult != nullptr, "Expected GUI-facing state to capture a validation result");
	RequireContains(cliValidation.Output, "\"operation\":\"" + guiResult->Operation + "\"", "Expected operation id parity");
	RequireContains(cliValidation.Output, "\"target\":\"" + guiResult->Target + "\"", "Expected target parity");
	RequireContains(cliValidation.Output, "\"status\":\"" + std::string(HE::ToString(guiResult->Status)) + "\"", "Expected status parity");
	RequireContains(cliValidation.Output, "\"summary\":\"" + guiResult->Summary + "\"", "Expected summary parity");
	RequireContains(cliValidation.Output, "\"can_continue_automatically\":" + std::string(guiResult->CanContinueAutomatically() ? "true" : "false"), "Expected continue parity");
	RequireContains(cliValidation.Output, "\"requires_manual_intervention\":" + std::string(guiResult->RequiresManualIntervention() ? "true" : "false"), "Expected manual intervention parity");

	for (const auto& [key, value] : guiResult->Payload) {
		RequireContains(
			cliValidation.Output,
			"\"" + key + "\":\"" + value + "\"",
			"Expected payload parity for key: " + key);
	}

	for (const auto& detail : guiResult->Details) {
		RequireContains(cliValidation.Output, "\"severity\":\"" + std::string(HE::ToString(detail.Severity)) + "\"", "Expected detail severity parity");
		RequireContains(cliValidation.Output, "\"code\":\"" + detail.Code + "\"", "Expected detail code parity");
		RequireContains(cliValidation.Output, "\"message\":\"" + detail.Message + "\"", "Expected detail message parity");
	}

	Require(guiResult->Payload.at("validated_domain_count") == std::to_string(validationReport.DomainCount), "Expected validated domain count parity");
	Require(guiResult->Payload.at("warning_count") == std::to_string(validationReport.WarningCount), "Expected warning count parity");
	Require(guiResult->Payload.at("error_count") == std::to_string(validationReport.ErrorCount), "Expected error count parity");

	std::filesystem::remove_all(smokeRoot, errorCode);
	Require(!errorCode, "Expected temporary cleanup to succeed");
	std::cout << "HostConsistencySmoke passed" << std::endl;
	return 0;
}
