#pragma once

#include <array>
#include <filesystem>
#include <string>

#include "HuaEngine.h"
#include "HuaEngine/Core/ResultEnvelope.h"
#include "Interaction/EditorInteractionHost.h"
#include "Interaction/EditorSceneCommands.h"
#include "HuaEngine/Project/ProjectContext.h"
#include "Panels/HierarchyPanel.h"
#include "Panels/InspectorPanel.h"
#include "Panels/ConsolePanel.h"
#include "Panels/ProjectPanel.h"
#include "Workbench/EditorWorkbenchState.h"
#include "Workbench/EditorSessionStorage.h"
#include "Workbench/ProjectSession.h"
#include "Workbench/SceneDocument.h"

namespace HE {
    enum class EditorWorkbenchMode {
        ProjectHub,
        WorkbenchShell
    };

    struct EditorLayerSpecification {
        bool BootstrapDemoScene = false;
        std::string WorkbenchProjectName = "EditorWorkbench";
        std::string InitialSceneName = "EditorWorkbench";
        std::filesystem::path StartupProjectPath;
        std::filesystem::path StartupScenePath;
    };

    enum class WorkbenchActionType {
        None,
        OpenProject,
        CloseProject,
        NewScene,
        OpenScene
    };

    struct WorkbenchActionRequest {
        WorkbenchActionType Type = WorkbenchActionType::None;
        std::filesystem::path Path;
        std::string Name;
        bool CreateIfMissing = false;
        bool SeedInitialScene = false;
    };

    class EditorLayer : public HE::Layer {
    public:
        explicit EditorLayer(EditorLayerSpecification specification = {});

        void OnAttach() override;
        void OnUpdate() override;
        void OnGuiRender() override;

    private:
        bool InitializeStartupProjectSession();
        bool OpenProjectFromPath(const std::filesystem::path& path, bool createIfMissing, std::string_view projectName, bool seedInitialScene);
        bool ActivateProjectSession(const ProjectContext& context, const ProjectStatusReport& status, bool seedInitialScene);
        bool ResumePersistedProjectSession();
        bool InitializeWorkbenchShell();
        bool BindSceneDocumentToShell();
        bool RestoreLastSceneForSession();
        bool BootstrapDemoScene();
        bool EnsureSandboxAssetsLoaded();
        bool SeedDemoProjectAssets();
        bool EnsureMeshAvailable(std::string_view meshName);
        bool EnsureMaterialAvailable(std::string_view materialName);
        bool WarmupSceneAssets(const Ref<Scene>& scene);
        bool CreateNewSceneDocument(std::string_view sceneName);
        bool OpenSceneDocument(const std::filesystem::path& scenePath);
        bool SaveActiveSceneDocument();
        bool SaveActiveSceneDocumentAs(const std::filesystem::path& scenePath);
        void SetSceneContext(const Ref<Scene>& scene);
        void SetSceneDocument(const Ref<Scene>& scene, const std::filesystem::path& scenePath, SceneDocumentSource source);
        void CreateSandboxEntities();
        void EnterProjectHub();
        void EnterWorkbenchShell();
        void CloseProjectSession(bool preserveResumeState, std::string_view summary);
        void SyncWorkbenchSessionState();
        void SyncSceneDocumentState();
        void RefreshInteractionHost();
        void RefreshCommandInputs();
        std::filesystem::path ResolveScenePathInput(const std::filesystem::path& scenePath) const;
        void PersistCurrentProjectSession();
        void ClearPersistedProjectSession();
        void RequestWorkbenchAction(const WorkbenchActionRequest& action);
        bool ExecuteWorkbenchAction(const WorkbenchActionRequest& action);
        void EnsureDefaultDockLayout(ImGuiID dockspaceId);
        void CaptureOperationResult(const ResultEnvelope& result);
        void RecordWorkbenchInfoEvent(std::string_view operation, std::string_view target, std::string_view summary);
        void RefreshWorkbenchValidation();
        bool ExecuteEditorCommand(EditorCommandPtr command);
        void ExecuteUndo();
        void ExecuteRedo();
        void CreateEntityFromHierarchy();
        void DeleteSelectedEntities();
        void AddComponentToPrimarySelection(EditorInspectableComponent type);
        void RemoveComponentFromPrimarySelection(EditorInspectableComponent type);
        void HandleGlobalShortcuts();
        std::string MakeDefaultEntityName() const;
        void OnUnsavedChangesPopup();
        void OnDockingPanel();
        void OnProjectHubPanel();
        void OnProjectHubShell();
        void OnScenePanel();

    private:
        EditorLayerSpecification m_Specification;
        EditorWorkbenchMode m_Mode = EditorWorkbenchMode::ProjectHub;
        ResultEnvelope m_LastOperationResult;
        EditorWorkbenchState m_WorkbenchState;
        EditorInteractionHost m_InteractionHost;
        std::filesystem::path m_WorkbenchRootPath;
        bool m_WorkbenchReady = false;
        bool m_DockLayoutInitialized = false;
        Ref<RenderTarget> m_RenderTarget;
        Ref<Rendering::EditorCamera> m_EditorCamera;
        Ref<Material> m_SandboxMaterial;
        Ref<ProjectPanel> m_ProjectPanel;
        Ref<HierarchyPanel> m_HierarchyPanel;
        Ref<InspectorPanel> m_Inspector;
        Ref<ConcolePanel> m_Concole;
        ProjectSession m_ProjectSession;
        SceneDocument m_SceneDocument;
        PersistedEditorSession m_PersistedSession;
        bool m_HasPersistedSession = false;
        WorkbenchActionRequest m_PendingAction;
        bool m_OpenUnsavedChangesPopup = false;
        std::array<char, 512> m_ProjectHubPathInput{};
        std::array<char, 128> m_ProjectHubNameInput{};
        std::array<char, 128> m_NewSceneNameInput{};
        std::array<char, 512> m_SceneOpenPathInput{};
        std::array<char, 512> m_SceneSaveAsPathInput{};
        bool m_ShowProjectPanel = true;
        bool m_ShowHierarchyPanel = true;
        bool m_ShowInspectorPanel = true;
        bool m_ShowConsolePanel = true;
        bool m_ShowScenePanel = true;

        glm::vec2 m_SceneViewportSize = { 0, 0 };
    };
}
