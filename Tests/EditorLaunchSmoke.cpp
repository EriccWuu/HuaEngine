#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "Launch/EditorLaunch.h"

namespace {
	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[EditorLaunchSmoke] " << message << std::endl;
			std::exit(1);
		}
	}
}

int main() {
	char executable[] = "Editor.exe";
	char* noProjectArguments[] = { executable };
	const auto projectHubLaunch = HE::Editor::ParseEditorLaunchOptions({ 1, noProjectArguments });
	Require(
		projectHubLaunch.Target == HE::Editor::EditorLaunchTarget::ProjectHub,
		"Expected an Editor launch without --project to delegate to ProjectHub");

	char projectFlag[] = "--project";
	char projectPath[] = "D:/Projects/Sample";
	char sceneFlag[] = "--scene";
	char scenePath[] = "Assets/Main.scene";
	char* workbenchArguments[] = { executable, projectFlag, projectPath, sceneFlag, scenePath };
	const auto workbenchLaunch = HE::Editor::ParseEditorLaunchOptions({ 5, workbenchArguments });
	Require(
		workbenchLaunch.Target == HE::Editor::EditorLaunchTarget::Workbench,
		"Expected --project to route directly to the Editor workbench");
	Require(
		workbenchLaunch.ProjectPath == std::filesystem::path(projectPath),
		"Expected the project path to be preserved");
	Require(
		workbenchLaunch.ScenePath == std::filesystem::path(scenePath),
		"Expected the scene path to be preserved");

	char unknownFlag[] = "--diagnostics";
	char* unknownArguments[] = { executable, unknownFlag };
	const auto unknownLaunch = HE::Editor::ParseEditorLaunchOptions({ 2, unknownArguments });
	Require(
		unknownLaunch.Target == HE::Editor::EditorLaunchTarget::Workbench,
		"Expected supplied arguments to remain in the Editor host");

	char* missingProjectArguments[] = { executable, projectFlag };
	const auto missingProjectLaunch = HE::Editor::ParseEditorLaunchOptions({ 2, missingProjectArguments });
	Require(
		missingProjectLaunch.Target == HE::Editor::EditorLaunchTarget::Workbench,
		"Expected an invalid --project argument to remain in the Editor recovery path");

	char* sceneOnlyArguments[] = { executable, sceneFlag, scenePath };
	const auto sceneOnlyLaunch = HE::Editor::ParseEditorLaunchOptions({ 3, sceneOnlyArguments });
	Require(
		sceneOnlyLaunch.Target == HE::Editor::EditorLaunchTarget::Workbench,
		"Expected --scene without --project to remain in the Editor recovery path");

	std::cout << "EditorLaunchSmoke passed" << std::endl;
	return 0;
}
