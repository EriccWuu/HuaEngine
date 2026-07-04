#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "HuaEngine.h"
#include "Workbench/EditorSessionStorage.h"
#include "Workbench/ProjectSession.h"
#include "Workbench/SceneDocument.h"

namespace {
	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[ProjectWorkbenchSmoke] " << message << std::endl;
			std::exit(1);
		}
	}

	HE::ApplicationSpecification MakeApplicationSpecification() {
		HE::ApplicationSpecification specification;
		specification.Name = "ProjectWorkbenchSmoke";
		specification.EnableGuiLayer = false;
		specification.EnableWindow = false;
		return specification;
	}

	class SmokeApplication final : public HE::Application {
	public:
		SmokeApplication()
			: HE::Application(MakeApplicationSpecification()) {}
	};

	HE::TransformComponent& GetFirstTransform(HE::Scene& scene) {
		HE::TransformComponent* firstTransform = nullptr;
		scene.GetWorld().Query<HE::TransformComponent>().ForEach([&](HE::Entity, HE::TransformComponent& transform) {
			if (firstTransform == nullptr) {
				firstTransform = &transform;
			}
		});
		Require(firstTransform != nullptr, "Expected at least one TransformComponent in the scene");
		return *firstTransform;
	}
}

int main() {
	char* originalLocalAppData = nullptr;
	size_t originalLocalAppDataLength = 0;
	_dupenv_s(&originalLocalAppData, &originalLocalAppDataLength, "LOCALAPPDATA");

	const auto smokeRoot = std::filesystem::temp_directory_path() / "HuaEngineProjectWorkbenchSmoke";
	const auto localAppDataRoot = smokeRoot / "LocalAppData";
	std::error_code errorCode;
	std::filesystem::remove_all(smokeRoot, errorCode);
	std::filesystem::create_directories(localAppDataRoot, errorCode);
	Require(!errorCode, "Expected LOCALAPPDATA root creation to succeed");
	_putenv_s("LOCALAPPDATA", localAppDataRoot.string().c_str());

	HE::Log::Init({ .EnableConsoleOutput = false });
	SmokeApplication application;
	application.Start();

	auto& operations = application.GetOperations();
	HE::ProjectContext context;
	auto initializeProject = operations.InitializeProject(smokeRoot / "WorkbenchProject", &context, "WorkbenchProject");
	Require(initializeProject.Succeeded(), "Expected project.initialize to succeed");

	HE::ProjectStatusReport status;
	auto projectStatus = operations.CheckProjectStatus(context, &status);
	Require(projectStatus.Succeeded() && status.IsOperational(), "Expected project.status to report an operational project");

	HE::ProjectSession session;
	session.Context = context;
	session.LastStatus = status;
	session.Loaded = true;

	HE::Ref<HE::Scene> scene;
	auto createScene = operations.CreateScene("WorkbenchScene", scene);
	Require(createScene.Succeeded() && scene, "Expected scene.create to succeed");

	auto entity = scene->GetWorld().CreateEntity();
	auto& transform = entity.GetComponent<HE::TransformComponent>();
	transform.Position = { 1.0f, 2.0f, 3.0f };

	HE::SceneDocument document;
	document.SceneRef = scene;
	document.DisplayName = "WorkbenchScene";
	document.MarkDirty();
	Require(document.Dirty, "Expected a new document to be markable as dirty");

	const auto scenePath = context.GetAssetRootPath() / "workbench_scene.scene";
	auto saveScene = operations.SaveScene(*scene, scenePath);
	Require(saveScene.Succeeded(), "Expected scene.save to succeed for the initial document");
	document.MarkSaved(scenePath);
	session.LastOpenedScenePath = scenePath;

	HE::PersistedEditorSession persisted;
	persisted.LastProjectRoot = context.RootPath.generic_string();
	persisted.LastProjectName = context.Descriptor.Name;
	persisted.LastScenePath = scenePath.generic_string();
	Require(HE::EditorSessionStorage::Save(persisted), "Expected editor session persistence to succeed");

	HE::PersistedEditorSession loadedSession;
	Require(HE::EditorSessionStorage::Load(loadedSession), "Expected persisted editor session to load");
	Require(loadedSession.LastProjectRoot == persisted.LastProjectRoot, "Expected persisted project root parity");
	Require(loadedSession.LastScenePath == persisted.LastScenePath, "Expected persisted last-scene parity");

	HE::Ref<HE::Scene> reopenedScene;
	auto loadScene = operations.LoadScene(session.LastOpenedScenePath, reopenedScene);
	Require(loadScene.Succeeded() && reopenedScene, "Expected scene.load to reopen the saved scene");
	auto& reopenedTransform = GetFirstTransform(*reopenedScene);
	Require(reopenedTransform.Position.x == 1.0f, "Expected reopened scene X position parity");
	Require(reopenedTransform.Position.y == 2.0f, "Expected reopened scene Y position parity");
	Require(reopenedTransform.Position.z == 3.0f, "Expected reopened scene Z position parity");

	reopenedTransform.Position.y = 9.0f;
	auto resaveScene = operations.SaveScene(*reopenedScene, session.LastOpenedScenePath);
	Require(resaveScene.Succeeded(), "Expected edited scene to save again");

	HE::Ref<HE::Scene> reloadedScene;
	auto reloadScene = operations.LoadScene(session.LastOpenedScenePath, reloadedScene);
	Require(reloadScene.Succeeded() && reloadedScene, "Expected scene.load to reopen the edited scene");
	auto& reloadedTransform = GetFirstTransform(*reloadedScene);
	Require(reloadedTransform.Position.y == 9.0f, "Expected edited scene data to persist after reopen");

	HE::EditorSessionStorage::Clear();
	Require(!std::filesystem::exists(HE::EditorSessionStorage::GetSessionFilePath()), "Expected editor session storage to clear persisted state");

	if (originalLocalAppData != nullptr) {
		_putenv_s("LOCALAPPDATA", originalLocalAppData);
		free(originalLocalAppData);
	} else {
		_putenv_s("LOCALAPPDATA", "");
	}

	HE::Log::GetCoreLogger().reset();
	HE::Log::GetClientLogger().reset();
	HE::Log::GetLogSink().reset();

	errorCode.clear();
	std::filesystem::remove_all(smokeRoot, errorCode);
	Require(!errorCode, "Expected smoke cleanup to succeed");

	std::cout << "ProjectWorkbenchSmoke passed" << std::endl;
	return 0;
}
