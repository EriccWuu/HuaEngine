#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "HuaEngine.h"
#include "Selection.h"
#include "Interaction/ContextMenuRegistry.h"
#include "Interaction/DragDropIntentRegistry.h"
#include "Interaction/EditorInteractionHost.h"
#include "Interaction/EditorSceneCommands.h"
#include "Interaction/ShortcutRegistry.h"
#include "Workbench/EditorWorkbenchState.h"
#include "Workbench/ProjectSession.h"
#include "Workbench/SceneDocument.h"
#include "imgui.h"

namespace {
    void Require(bool condition, const std::string& message) {
        if (!condition) {
            std::cerr << "[EditorInteractionSmoke] " << message << std::endl;
            std::exit(1);
        }
    }

    HE::ApplicationSpecification MakeApplicationSpecification() {
        HE::ApplicationSpecification specification;
        specification.Name = "EditorInteractionSmoke";
        specification.EnableGuiLayer = false;
        specification.EnableWindow = false;
        return specification;
    }

    class SmokeApplication final : public HE::Application {
    public:
        SmokeApplication()
            : HE::Application(MakeApplicationSpecification()) {}
    };

    size_t CountEntities(HE::Scene& scene) {
        auto& registry = scene.GetEntityManager().GetRegistry();
        size_t count = 0;
        for (auto entityHandle : registry.view<HE::TransformComponent>()) {
            (void)entityHandle;
            ++count;
        }
        return count;
    }
}

int main() {
    HE::Log::Init({ .EnableConsoleOutput = false });
    SmokeApplication application;
    application.Start();

    const auto smokeRoot = std::filesystem::temp_directory_path() / "HuaEngineEditorInteractionSmoke";
    std::error_code errorCode;
    std::filesystem::remove_all(smokeRoot, errorCode);

    auto& operations = application.GetOperations();
    HE::ProjectContext context;
    auto initializeProject = operations.InitializeProject(smokeRoot / "InteractionProject", &context, "InteractionProject");
    Require(initializeProject.Succeeded(), "Expected project.initialize to succeed for editor interaction smoke");

    HE::ProjectStatusReport status;
    auto checkStatus = operations.CheckProjectStatus(context, &status);
    Require(checkStatus.Succeeded() && status.IsOperational(), "Expected project.status to report an operational project");

    HE::Ref<HE::Scene> scene;
    auto createScene = operations.CreateScene("InteractionScene", scene);
    Require(createScene.Succeeded() && scene, "Expected scene.create to succeed for editor interaction smoke");

    HE::EditorWorkbenchState workbenchState;
    HE::ProjectSession projectSession;
    projectSession.Context = context;
    projectSession.LastStatus = status;
    projectSession.Loaded = true;

    HE::SceneDocument sceneDocument;
    sceneDocument.SceneRef = scene;
    sceneDocument.DisplayName = "InteractionScene";

    HE::EditorInteractionHost interactionHost;
    interactionHost.Bind(&workbenchState, &projectSession, &sceneDocument);
    interactionHost.ResetCommandHistory(true);
    HE::Selection::ClearSelection();

    bool shortcutTriggered = false;
    interactionHost.Shortcuts().Register({
        .CommandId = "editor.test.undo",
        .DisplayName = "Undo",
        .Chord = ImGuiMod_Ctrl | ImGuiKey_Z,
        .Shortcut = "Ctrl+Z",
        .IsEnabled = []() { return true; },
        .Trigger = [&shortcutTriggered]() { shortcutTriggered = true; }
    });
    const auto* shortcutBinding = interactionHost.Shortcuts().Find("editor.test.undo");
    Require(shortcutBinding != nullptr, "Expected shortcut registry to return the registered binding");

    bool contextTriggered = false;
    interactionHost.ContextMenus().Replace("hierarchy.entity", {
        {
            .Id = "entity.create",
            .Label = "Create Entity",
            .Shortcut = "Ctrl+Shift+N",
            .Tooltip = "Create a test entity",
            .Enabled = true,
            .IsEnabled = []() { return true; },
            .Trigger = [&contextTriggered]() { contextTriggered = true; }
        }
    });
    const auto* contextActions = interactionHost.ContextMenus().Find("hierarchy.entity");
    Require(contextActions != nullptr && contextActions->size() == 1, "Expected context menu registry to expose the registered action");
    (*contextActions)[0].Trigger();
    Require(contextTriggered, "Expected context menu trigger to invoke the registered callback");

    interactionHost.DragDrop().Register({
        .Id = "hierarchy.entity.reorder",
        .Label = "Hierarchy Entity",
        .PayloadType = "HE_HIERARCHY_ENTITY",
        .Source = "hierarchy.entity",
        .Target = "hierarchy.entity",
        .Enabled = true
    });
    Require(interactionHost.DragDrop().Find("hierarchy.entity", "hierarchy.entity") != nullptr, "Expected drag-drop registry to resolve the registered intent");

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(1280.0f, 720.0f);
    io.Fonts->AddFontDefault();
    unsigned char* fontPixels = nullptr;
    int fontWidth = 0;
    int fontHeight = 0;
    io.Fonts->GetTexDataAsRGBA32(&fontPixels, &fontWidth, &fontHeight);
    io.AddKeyEvent(ImGuiMod_Ctrl, true);
    io.AddKeyEvent(ImGuiKey_LeftCtrl, true);
    io.AddKeyEvent(ImGuiKey_Z, true);
    ImGui::NewFrame();
    interactionHost.Shortcuts().DispatchTriggered();
    ImGui::EndFrame();
    Require(shortcutTriggered, "Expected shortcut dispatch to trigger the registered Ctrl+Z callback");
    ImGui::DestroyContext();

    auto createEntityResult = interactionHost.ExecuteCommand(HE::CreateCreateEntityCommand("Smoke Entity 1"));
    Require(createEntityResult.Succeeded(), "Expected create-entity command to succeed");
    Require(HE::Selection::HasSingleSelection(), "Expected create-entity command to select the new entity");
    Require(sceneDocument.Dirty, "Expected command execution to mark the scene document dirty");
    Require(CountEntities(*scene) == 1, "Expected exactly one entity after the first create command");

    auto firstEntity = HE::Selection::GetPrimarySelection();
    auto addCameraResult = interactionHost.ExecuteCommand(HE::CreateAddComponentCommand(HE::EditorInspectableComponent::Camera, firstEntity));
    Require(addCameraResult.Succeeded(), "Expected add-camera command to succeed");
    Require(firstEntity.HasComponent<HE::Rendering::CameraComponent>(), "Expected CameraComponent to exist after add-camera");

    auto removeCameraResult = interactionHost.ExecuteCommand(HE::CreateRemoveComponentCommand(HE::EditorInspectableComponent::Camera, firstEntity));
    Require(removeCameraResult.Succeeded(), "Expected remove-camera command to succeed");
    Require(!firstEntity.HasComponent<HE::Rendering::CameraComponent>(), "Expected CameraComponent to be removed");

    auto undoRemoveCamera = interactionHost.Undo();
    Require(undoRemoveCamera.Succeeded(), "Expected undo after remove-camera to succeed");
    Require(firstEntity.HasComponent<HE::Rendering::CameraComponent>(), "Expected undo to restore CameraComponent");

    auto createSecondEntityResult = interactionHost.ExecuteCommand(HE::CreateCreateEntityCommand("Smoke Entity 2"));
    Require(createSecondEntityResult.Succeeded(), "Expected second create-entity command to succeed");
    auto secondEntity = HE::Selection::GetPrimarySelection();
    Require(CountEntities(*scene) == 2, "Expected two entities after the second create command");

    HE::Selection::SetSelections({ firstEntity, secondEntity });
    auto deleteEntitiesResult = interactionHost.ExecuteCommand(HE::CreateDeleteEntitiesCommand(HE::Selection::GetSelections()));
    Require(deleteEntitiesResult.Succeeded(), "Expected delete-selected command to succeed");
    Require(!HE::Selection::HasSelection(), "Expected delete-selected command to clear the selection");
    Require(CountEntities(*scene) == 0, "Expected delete-selected command to remove both entities");

    auto undoDelete = interactionHost.Undo();
    Require(undoDelete.Succeeded(), "Expected undo after delete-selected to succeed");
    Require(HE::Selection::Count() == 2, "Expected undo after delete-selected to restore both selections");
    Require(CountEntities(*scene) == 2, "Expected undo after delete-selected to restore both entities");

    interactionHost.MarkSaved();
    Require(!sceneDocument.Dirty, "Expected mark-saved to clear the document dirty flag");

    std::filesystem::remove_all(smokeRoot, errorCode);
    Require(!errorCode, "Expected editor interaction smoke cleanup to succeed");

    std::cout << "EditorInteractionSmoke passed" << std::endl;
    return 0;
}
