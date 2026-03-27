#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "HuaEngine.h"
#include "HuaEngine/Project/ProjectService.h"

namespace {
	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[ProjectServiceSmoke] " << message << std::endl;
			std::exit(1);
		}
	}

	std::string ReadFileText(const std::filesystem::path& path) {
		std::ifstream stream(path, std::ios::in | std::ios::binary);
		Require(stream.good(), "Expected file read to succeed: " + path.generic_string());
		return std::string((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
	}
}

int main() {
	HE::Log::Init();
	HE::Serialization::InitializeSerialization();

	const auto testRoot = std::filesystem::temp_directory_path() / "HuaEngineProjectServiceSmoke";
	std::error_code errorCode;
	std::filesystem::remove_all(testRoot, errorCode);

	HE::ProjectService projectService;
	HE::ProjectContext context;

	auto initializeResult = projectService.InitializeProject(testRoot / "SmokeProject", &context, "SmokeProject");
	Require(initializeResult.Succeeded(), "Expected project.initialize to succeed");
	Require(context.IsLoaded(), "Expected initialized project context to be loaded");
	Require(std::filesystem::exists(context.ProjectFilePath), "Expected project metadata file to exist");
	Require(std::filesystem::exists(context.GetAssetRootPath()), "Expected asset directory to exist");
	Require(std::filesystem::exists(context.GetSceneRootPath()), "Expected scene directory to exist");

	const auto projectFileText = ReadFileText(context.ProjectFilePath);
	Require(projectFileText.find("\"name\"") != std::string::npos, "Expected project metadata to use modern lower_snake field names");
	Require(projectFileText.find("\"schema_version\"") != std::string::npos, "Expected project metadata to persist schema_version");
	Require(projectFileText.find("\"Name\"") == std::string::npos, "Expected project metadata to avoid legacy PascalCase field names");

	HE::ProjectStatusReport operationalStatus;
	auto statusResult = projectService.CheckProjectStatus(context, &operationalStatus);
	Require(statusResult.Succeeded(), "Expected project.status to report an operational project");
	Require(operationalStatus.IsOperational(), "Expected project status report to be operational");

	HE::ProjectContext resolvedContext;
	auto resolveResult = projectService.ResolveProjectContext(context.GetSceneRootPath(), resolvedContext);
	Require(resolveResult.Succeeded(), "Expected project.resolve_context to succeed from a nested managed directory");
	Require(resolvedContext.RootPath == context.RootPath, "Expected resolved project root to match the initialized root");

	std::filesystem::remove_all(context.GetSceneRootPath(), errorCode);
	Require(!errorCode, "Expected scene directory removal to succeed for status degradation check");

	std::ofstream replacementSceneFile(context.GetSceneRootPath());
	Require(replacementSceneFile.good(), "Expected replacement scene file to be created for invalid-type status check");
	replacementSceneFile << "not a directory";
	replacementSceneFile.close();

	HE::ProjectStatusReport degradedStatus;
	auto degradedResult = projectService.CheckProjectStatus(context, &degradedStatus);
	Require(degradedResult.RequiresManualIntervention(), "Expected invalid managed directory types to require manual intervention");
	Require(!degradedStatus.IsOperational(), "Expected degraded project status to reject invalid managed directory types");

	std::filesystem::remove_all(testRoot, errorCode);
	Require(!errorCode, "Expected smoke test temporary directory cleanup to succeed");

	std::cout << "ProjectServiceSmoke passed" << std::endl;
	return 0;
}
