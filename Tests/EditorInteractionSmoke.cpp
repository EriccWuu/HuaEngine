#include <array>
#include "ECSTestSupport.h"
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "HuaEngine.h"
#include "Selection.h"
#include "Selection/EditorSelectionService.h"
#include "Assets/AssetPickerCatalog.h"
#include "Interaction/ContextMenuRegistry.h"
#include "Interaction/DragDropIntentRegistry.h"
#include "Interaction/EditorInteractionHost.h"
#include "Interaction/EditorSceneCommands.h"
#include "Input/EditorInputService.h"
#include "Scene/SceneEntityInspectorEditor.h"
#include "Workbench/EditorWorkbenchState.h"
#include "Workbench/ProjectSession.h"
#include "Workbench/SceneDocument.h"
#include "Viewport/EditorCameraController.h"
#include "Viewport/EditorGrid.h"
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
        return scene.GetWorld().EntityCount();
    }
}

int main() {
	HE::Editor::AssetPickerCatalog pickerCatalog;
	HE::Editor::SceneEntityInspectorEditor sceneInspector(pickerCatalog);
	sceneInspector.BindInteractionHost(nullptr);
	Require(!sceneInspector.HasEditingContext(), "Expected no scene editing context without host");
	Require(sceneInspector.ResolveTypeRegistry() == nullptr, "Expected no borrowed component registry without a scene");

	HE::Editor::EditorSelectionService selectionService;
	const HE::EntityUuid firstUuid{ 1, 2 };
	const HE::EntityUuid secondUuid{ 3, 4 };
	selectionService.SelectEntities({ firstUuid, secondUuid });
	Require(selectionService.HasEntitySelection(), "Expected entity selection state");
	Require(selectionService.GetEntitySelection()->Entities.size() == 2, "Expected all selected entity UUIDs");
	selectionService.SelectAsset("asset-guid");
	Require(selectionService.HasAssetSelection(), "Expected asset selection state");
	Require(!selectionService.HasEntitySelection(), "Expected asset selection to replace entity selection");
	selectionService.SelectEntities({ firstUuid });
	Require(selectionService.HasEntitySelection() && !selectionService.HasAssetSelection(), "Expected entity selection to replace asset selection");
	selectionService.Clear();
	Require(!selectionService.HasSelection(), "Expected clear to remove editor selection");
	bool selectionGuardCalled = false;
	selectionService.SelectAsset("guarded-asset");
	selectionService.SetChangeGuard([&selectionGuardCalled](const HE::Editor::EditorSelection&) {
		selectionGuardCalled = true;
		return false;
	});
	selectionService.SelectEntities({ firstUuid });
	Require(selectionGuardCalled && selectionService.HasAssetSelection(), "Expected rejected selection change to preserve the active asset");
	selectionService.AcceptGuardedSelection(HE::Editor::EntitySelection{ { firstUuid } });
	Require(selectionService.HasEntitySelection(), "Expected confirmed selection change to bypass the guard");

	HE::Selection::SelectAsset("facade-asset");
	Require(HE::Selection::HasAssetSelection(), "Expected compatibility facade asset selection");
	Require(!HE::Selection::HasSelection(), "Expected entity-only compatibility query to exclude asset selection");
	HE::Selection::SetSelectedEntities({ firstUuid });
	Require(HE::Selection::HasSingleSelection() && !HE::Selection::HasAssetSelection(), "Expected compatibility facade entity selection to replace asset selection");
	HE::Selection::ClearSelection();

    const glm::vec3 distantCameraPosition(513.0f, 200.0f, -777.0f);
    const auto gridLayout = HE::Editor::CalculateEditorGridLayout(distantCameraPosition);
    Require(
        glm::abs(gridLayout.Center.x - distantCameraPosition.x) <= gridLayout.Spacing * 0.5f
            && glm::abs(gridLayout.Center.y - distantCameraPosition.z) <= gridLayout.Spacing * 0.5f,
        "Expected editor grid to remain centered near a camera far from the world origin");
    Require(
        gridLayout.Spacing > 1.0f && gridLayout.HalfExtent > distantCameraPosition.y,
        "Expected editor grid spacing and coverage to scale with camera distance");

    HE::Editor::EditorCameraController editorCamera;
    editorCamera.SetPose(distantCameraPosition, 0.0f, 0.0f);
    const auto renderCamera = editorCamera.BuildRenderCamera();
    const glm::vec4 distantPoint = renderCamera.GetViewProjection()
        * glm::vec4(distantCameraPosition + editorCamera.GetForwardDirection() * 1000.0f, 1.0f);
    Require(
        distantPoint.w > 0.0f && glm::abs(distantPoint.z / distantPoint.w) <= 1.0f,
        "Expected the default editor camera far plane to include distant scene references");

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
    sceneInspector.BindInteractionHost(&interactionHost);
    const auto* initialRegistry = &scene->GetWorld().Types();
    Require(sceneInspector.ResolveTypeRegistry() == initialRegistry,
        "Expected Inspector to borrow the active World's Context registry");
    {
        HE::Ecs::EcsContext alternateContext;
        const auto reflectedTypes = HE::Refl::GetRuntimeTypes();
        for (auto iterator = reflectedTypes.rbegin(); iterator != reflectedTypes.rend(); ++iterator) {
            if (iterator->Kind != "component") continue;
            Require(iterator->MakeEcsType != nullptr, "Expected a shared ECS component descriptor factory");
            Require(alternateContext.Types().Register(iterator->MakeEcsType()).HasValue(),
                "Expected reverse component registration for Inspector Context switching");
        }
        HE::SceneDocument alternateDocument;
        alternateDocument.SceneRef = HE::CreateRef<HE::Scene>(alternateContext, "Alternate Inspector Scene");
        interactionHost.Bind(&workbenchState, &projectSession, &alternateDocument);
        const auto* alternateRegistry = sceneInspector.ResolveTypeRegistry();
        Require(alternateRegistry == &alternateContext.Types() && alternateRegistry != initialRegistry,
            "Expected Inspector scene switching to resolve the new Context without a cached registry");
        const auto* metadata = alternateRegistry->Find<HE::TransformComponent>();
        Require(metadata != nullptr && metadata == alternateContext.Types().Find<HE::TransformComponent>(),
            "Expected Inspector metadata to belong to the selected Context");
        interactionHost.Bind(&workbenchState, &projectSession, &sceneDocument);
        Require(sceneInspector.ResolveTypeRegistry() == initialRegistry,
            "Expected Inspector to restore the first Context when switching back");
    }
    interactionHost.ResetCommandHistory(true);
    HE::Selection::ClearSelection();

    bool commandTriggered = false;
    Require(interactionHost.Input().Commands().Register({
        .Id = "editor.test.undo",
        .DisplayName = "Undo",
        .Category = "Test",
        .CanExecute = []() { return true; },
        .Execute = [&commandTriggered]() { commandTriggered = true; }
    }).Succeeded(), "Expected editor command registration");
    Require(interactionHost.Input().Bindings().RegisterDefaultCommand({
        "test.undo",
        "editor.test.undo",
        "Global",
        { HE::KeyboardControl(HE::Key::Z), HE::InputModifiers::Control, HE::InputTrigger::Pressed, true },
        0,
        true
    }).Succeeded(), "Expected editor command binding registration");
    Require(interactionHost.Input().Commands().Find("editor.test.undo") != nullptr, "Expected command registry lookup");
    Require(interactionHost.Input().Bindings().GetDisplayText("editor.test.undo") == "Ctrl+Z", "Expected shortcut display from binding metadata");

    interactionHost.ContextMenus().Replace("hierarchy.entity", {
        {
            .CommandId = "editor.test.undo",
            .Label = "Create Entity",
            .Tooltip = "Create a test entity",
            .Enabled = true
        }
    });
    const auto* contextActions = interactionHost.ContextMenus().Find("hierarchy.entity");
    Require(contextActions != nullptr && contextActions->size() == 1, "Expected context menu registry to expose the registered action");
    Require((*contextActions)[0].CommandId == "editor.test.undo", "Expected context menu to reference the shared command");

    interactionHost.DragDrop().Register({
        .Id = "hierarchy.entity.reorder",
        .Label = "Hierarchy Entity",
        .PayloadType = "HE_HIERARCHY_ENTITY",
        .Source = "hierarchy.entity",
        .Target = "hierarchy.entity",
        .Enabled = true
    });
    Require(interactionHost.DragDrop().Find("hierarchy.entity", "hierarchy.entity") != nullptr, "Expected drag-drop registry to resolve the registered intent");

    HE::InputSystem commandInput;
    commandInput.BeginFrame();
    commandInput.Submit(HE::RawInputEvent::Key(HE::Key::Z, HE::InputPhase::Pressed, HE::InputModifiers::Control));
    interactionHost.Input().Contexts().BeginFrame();
    Require(interactionHost.Input().Resolve(commandInput.FinalizeFrame()).Succeeded(), "Expected contextual command resolution");
    Require(commandTriggered, "Expected Ctrl+Z to execute the shared command");

    auto createEntityResult = interactionHost.ExecuteCommand(HE::CreateCreateEntityCommand("Smoke Entity 1"));
    Require(createEntityResult.Succeeded(), "Expected create-entity command to succeed");
    Require(HE::Selection::HasSingleSelection(), "Expected create-entity command to select the new entity");
    Require(HE::Selection::GetSelectedEntityUuid() != HE::EntityUuid{}, "Expected create-entity command to store the selection uuid");
    Require(sceneDocument.Dirty, "Expected command execution to mark the scene document dirty");
    Require(CountEntities(*scene) == 1, "Expected exactly one entity after the first create command");

    auto firstEntity = HE::Selection::ResolvePrimarySelection(scene->GetWorld());
    auto addCameraResult = interactionHost.ExecuteCommand(HE::CreateAddComponentCommand(HE::EditorInspectableComponent::Camera, scene->GetWorld(), firstEntity));
    Require(addCameraResult.Succeeded(), "Expected add-camera command to succeed");
    Require(scene->GetWorld().Has<HE::Rendering::CameraComponent>(firstEntity), "Expected CameraComponent to exist after add-camera");

    auto removeCameraResult = interactionHost.ExecuteCommand(HE::CreateRemoveComponentCommand(HE::EditorInspectableComponent::Camera, scene->GetWorld(), firstEntity));
    Require(removeCameraResult.Succeeded(), "Expected remove-camera command to succeed");
    Require(!scene->GetWorld().Has<HE::Rendering::CameraComponent>(firstEntity), "Expected CameraComponent to be removed");

    auto undoRemoveCamera = interactionHost.Undo();
    Require(undoRemoveCamera.Succeeded(), "Expected undo after remove-camera to succeed");
    Require(scene->GetWorld().Has<HE::Rendering::CameraComponent>(firstEntity), "Expected undo to restore CameraComponent");

    Require(interactionHost.ExecuteCommand(HE::CreateAddComponentCommand(HE::EditorInspectableComponent::Material, scene->GetWorld(), firstEntity)).Succeeded(), "Expected add-material command to succeed");
    HE::Rendering::MaterialOverrideSet beforeOverrides;
    HE::Rendering::MaterialOverrideSet afterOverrides;
    afterOverrides.SetVec4("u_Color", glm::vec4(0.2f, 0.4f, 0.6f, 1.0f));
    Require(interactionHost.ExecuteCommand(HE::CreateSetMaterialOverridesCommand(scene->GetWorld(), firstEntity, beforeOverrides, afterOverrides)).Succeeded(), "Expected material override command to succeed");
    Require(ECSTestSupport::Get<HE::Rendering::MaterialComponent>(scene->GetWorld(), firstEntity).Overrides.Parameters.contains("u_Color"), "Expected material override command to apply");
    Require(interactionHost.Undo().Succeeded() && ECSTestSupport::Get<HE::Rendering::MaterialComponent>(scene->GetWorld(), firstEntity).Overrides.Empty(), "Expected material override undo to restore the prior value");
    Require(interactionHost.Redo().Succeeded() && ECSTestSupport::Get<HE::Rendering::MaterialComponent>(scene->GetWorld(), firstEntity).Overrides.Parameters.contains("u_Color"), "Expected material override redo to reapply the value");

    const auto firstIdentity = scene->GetWorld().Uuid(firstEntity);
    const auto* transformType = scene->GetWorld().Types().Find<HE::TransformComponent>();
    auto beforeTransform = ECSTestSupport::Take(HE::Ecs::OwnedValue::Copy(*transformType,
        scene->GetWorld().TryGet<HE::TransformComponent>(firstEntity)));
    auto afterTransform = ECSTestSupport::Take(beforeTransform.Clone());
    static_cast<HE::TransformComponent*>(afterTransform.Data())->Position = {7, 11, 13};
    Require(interactionHost.ExecuteCommand(HE::CreateSetComponentValueCommand(scene, firstEntity,
        std::move(beforeTransform), std::move(afterTransform))).Succeeded(), "Expected generic reflected component edit");
    Require(ECSTestSupport::Get<HE::TransformComponent>(scene->GetWorld(), firstEntity).Position == glm::vec3(7, 11, 13),
        "Expected generic owned edit payload to reach runtime storage");
    Require(interactionHost.Undo().Succeeded() &&
        ECSTestSupport::Get<HE::TransformComponent>(scene->GetWorld(), firstEntity).Position == glm::vec3(0),
        "Expected generic field undo to restore the entire owned component");
    Require(interactionHost.Redo().Succeeded(), "Expected generic field redo");
    {
        auto before = ECSTestSupport::Take(HE::Ecs::OwnedValue::Copy(*transformType,
            scene->GetWorld().TryGet<HE::TransformComponent>(firstEntity)));
        auto after = ECSTestSupport::Take(before.Clone());
        auto oldSceneCommand = HE::CreateSetComponentValueCommand(scene, firstEntity, std::move(before), std::move(after));
        HE::SceneDocument alternate;
        alternate.SceneRef = HE::CreateRef<HE::Scene>("Other command target");
        interactionHost.Bind(&workbenchState, &projectSession, &alternate);
        Require(!interactionHost.ExecuteCommand(std::move(oldSceneCommand)).Succeeded(),
            "Expected a captured component command to reject a different scene");
        interactionHost.Bind(&workbenchState, &projectSession, &sceneDocument);
    }

    auto createSecondEntityResult = interactionHost.ExecuteCommand(HE::CreateCreateEntityCommand("Smoke Entity 2"));
    Require(createSecondEntityResult.Succeeded(), "Expected second create-entity command to succeed");
    auto secondEntity = HE::Selection::ResolvePrimarySelection(scene->GetWorld());
    Require(CountEntities(*scene) == 2, "Expected two entities after the second create command");

    const std::array selectedIds{firstEntity, secondEntity};
    HE::Selection::SetSelections(scene->GetWorld(), selectedIds);
    auto deleteEntitiesResult = interactionHost.ExecuteCommand(HE::CreateDeleteEntitiesCommand(scene->GetWorld(), HE::Selection::ResolveSelections(scene->GetWorld())));
    Require(deleteEntitiesResult.Succeeded(), "Expected delete-selected command to succeed");
    Require(!HE::Selection::HasSelection(), "Expected delete-selected command to clear the selection");
    Require(CountEntities(*scene) == 0, "Expected delete-selected command to remove both entities");

    auto undoDelete = interactionHost.Undo();
    Require(undoDelete.Succeeded(), "Expected undo after delete-selected to succeed");
    Require(HE::Selection::Count() == 2, "Expected undo after delete-selected to restore both selections");
    Require(CountEntities(*scene) == 2, "Expected undo after delete-selected to restore both entities");

    Require(scene->GetWorld().Find(firstIdentity) != HE::EntityId{} && !scene->GetWorld().IsAlive(firstEntity),
        "Expected undo deletion to preserve UUID while allocating a new generation");
    const auto restoredFirst = scene->GetWorld().Find(firstIdentity);
    Require(ECSTestSupport::Get<HE::TransformComponent>(scene->GetWorld(), restoredFirst).Position == glm::vec3(7, 11, 13),
        "Expected delete undo to restore edited component values");

    interactionHost.MarkSaved();
    Require(!sceneDocument.Dirty, "Expected mark-saved to clear the document dirty flag");

    std::filesystem::remove_all(smokeRoot, errorCode);
    Require(!errorCode, "Expected editor interaction smoke cleanup to succeed");

    std::cout << "EditorInteractionSmoke passed" << std::endl;
    return 0;
}
