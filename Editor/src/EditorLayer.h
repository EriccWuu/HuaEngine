#pragma once

#include <filesystem>

#include "HuaEngine.h"
#include "HuaEngine/Core/ResultEnvelope.h"
#include "HuaEngine/Project/ProjectContext.h"
#include "Panels/SceneHierarchyPanel.h"
#include "Panels/InspectorPanel.h"
#include "Panels/ConsolePanel.h"
#include "Workbench/EditorWorkbenchState.h"

namespace HE {
    struct EditorLayerSpecification {
        bool BootstrapDemoScene = false;
        std::string WorkbenchProjectName = "EditorWorkbench";
        std::string InitialSceneName = "EditorWorkbench";
    };

    class EditorLayer : public HE::Layer {
    public:
        explicit EditorLayer(EditorLayerSpecification specification = {});

        void OnAttach() override;
        void OnUpdate() override;
        void OnGuiRender() override;

    private:
        bool InitializeWorkbenchContext();
        bool InitializeWorkbenchShell();
        bool BootstrapDemoScene();
        bool EnsureSandboxAssetsLoaded();
        void SetSceneContext(const Ref<Scene>& scene);
        void CreateSandboxEntities();
        void EnsureDefaultDockLayout(ImGuiID dockspaceId);
        void CaptureOperationResult(const ResultEnvelope& result);
        void RefreshWorkbenchValidation();
        void OnDockingPanel();
        void OnScenePanel();

    private:
        EditorLayerSpecification m_Specification;
        ResultEnvelope m_LastOperationResult;
        EditorWorkbenchState m_WorkbenchState;
        ProjectContext m_ProjectContext;
        std::filesystem::path m_WorkbenchRootPath;
        bool m_WorkbenchReady = false;
        bool m_DockLayoutInitialized = false;
        Ref<FrameBuffer> m_FrameBuffer;
        Ref<Rendering::EditorCamera> m_EditorCamera;
        Ref<Scene> m_Scene;
        Ref<Material> m_SandboxMaterial;
        Ref<SceneHierarchyPanel> m_SceneHierarchy;
        Ref<InspectorPanel> m_Inspector;
        Ref<ConcolePanel> m_Concole;

        glm::vec2 m_SceneViewportSize = { 0, 0 };
    };
}
