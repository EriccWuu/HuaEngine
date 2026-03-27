#include "enginepch.h"
#include "EditorLayer.h"

#include <filesystem>

#include "HuaEngine/Application/ApplicationOperations.h"
#include "imgui.h"
#include <imgui_internal.h>

namespace HE {
    EditorLayer::EditorLayer(EditorLayerSpecification specification)
        : Layer("EditorLayer"), m_Specification(std::move(specification)) {
        // Initialize camera
        m_EditorCamera.reset(new Rendering::EditorCamera());
        m_Inspector.reset(new InspectorPanel);
        m_Concole.reset(new ConcolePanel);
    }

    void EditorLayer::OnAttach() {
        if (!InitializeWorkbenchContext()) {
            return;
        }

        if (m_Specification.BootstrapDemoScene) {
            if (!BootstrapDemoScene()) {
                return;
            }
        } else {
            Ref<Scene> emptyScene;
            auto& operations = Application::GetInstance().GetOperations();
            CaptureOperationResult(operations.CreateScene(m_Specification.InitialSceneName, emptyScene));
            if (!m_LastOperationResult.Succeeded() || !emptyScene) {
                return;
            }

            SetSceneContext(emptyScene);
        }

        if (!InitializeWorkbenchShell()) {
            return;
        }

        m_WorkbenchReady = true;
    }

    bool EditorLayer::InitializeWorkbenchContext() {
        auto& operations = Application::GetInstance().GetOperations();
        const char* localAppData = std::getenv("LOCALAPPDATA");
        if (localAppData && *localAppData) {
            m_WorkbenchRootPath = std::filesystem::path(localAppData) / "HuaEngine" / "Workbench";
        } else {
            m_WorkbenchRootPath = std::filesystem::temp_directory_path() / "huaengine_editor_workbench";
        }

        auto initializeProject = [&]() {
            return operations.InitializeProject(
                m_WorkbenchRootPath,
                &m_ProjectContext,
                m_Specification.WorkbenchProjectName);
        };

        CaptureOperationResult(initializeProject());
        if (!m_LastOperationResult.Succeeded()) {
            std::error_code errorCode;
            std::filesystem::remove_all(m_WorkbenchRootPath, errorCode);
            if (errorCode) {
                auto result = ResultEnvelope::Failure("editor.workbench.initialize_context", m_WorkbenchRootPath.generic_string(), "Failed to reset unreadable editor workbench root");
                result.AddDetail({ DiagnosticSeverity::Error, "editor.workbench.root_reset_failed", errorCode.message(), m_WorkbenchRootPath.generic_string() });
                CaptureOperationResult(result);
                return false;
            }

            CaptureOperationResult(initializeProject());
            if (!m_LastOperationResult.Succeeded()) {
                return false;
            }
        }

        CaptureOperationResult(operations.CheckProjectStatus(m_ProjectContext));
        if (!m_LastOperationResult.Succeeded()) {
            return false;
        }

        m_Inspector->SetWorkbenchState(&m_WorkbenchState);
        m_Concole->SetWorkbenchState(&m_WorkbenchState);
        return true;
    }

    void EditorLayer::SetSceneContext(const Ref<Scene>& scene) {
        m_Scene = scene;

        if (!m_SceneHierarchy) {
            m_SceneHierarchy.reset(new SceneHierarchyPanel(m_Scene));
            m_SceneHierarchy->SetWorkbenchState(&m_WorkbenchState);
        } else {
            m_SceneHierarchy->SetContext(m_Scene);
        }
    }

    bool EditorLayer::InitializeWorkbenchShell() {
        if (!m_Scene) {
            auto result = ResultEnvelope::Failure("editor.workbench.initialize_shell", "editor.workbench", "Workbench scene context is incomplete");
            result.AddDetail({ DiagnosticSeverity::Error, "editor.workbench.missing_context", "Scene is not ready", {} });
            CaptureOperationResult(result);
            return false;
        }

        // The workbench shell owns the scene viewport and panel wiring even without demo content.
        FrameBufferSpecification spec;
        spec.Width = 1280;
        spec.Height = 720;
        spec.Attachments = { FrameBufferTextureFormat::RGBA8 };
        m_FrameBuffer = FrameBuffer::Create(spec);

        auto& operations = Application::GetInstance().GetOperations();
        CaptureOperationResult(operations.AttachSceneViewportRenderer(m_Scene, m_FrameBuffer));
        if (!m_LastOperationResult.Succeeded()) {
            return false;
        }

        RefreshWorkbenchValidation();
        return true;
    }

    bool EditorLayer::EnsureSandboxAssetsLoaded() {
        MeshManager::Instance().LoadDefaultMeshes();

        auto customMesh = Mesh::LoadFromFile("assets/CustomMesh.mesh");
        if (!customMesh) {
            auto result = ResultEnvelope::Failure("editor.workbench.bootstrap_scene", "assets/CustomMesh.mesh", "Failed to load custom sandbox mesh");
            result.AddDetail({ DiagnosticSeverity::Error, "editor.workbench.bootstrap_scene.mesh_load_failed", "CustomMesh.mesh could not be loaded from assets", {} });
            CaptureOperationResult(result);
            return false;
        }
        MeshManager::Instance().RegisterMesh("CustomSquare", customMesh);

        m_SandboxMaterial = Material::Create("SandboxMaterial");
        if (!Serialization::LoadMaterial("assets/SandboxMaterial.material", *m_SandboxMaterial)) {
            auto result = ResultEnvelope::Failure("editor.workbench.bootstrap_scene", "assets/SandboxMaterial.material", "Failed to load sandbox material");
            result.AddDetail({ DiagnosticSeverity::Error, "editor.workbench.bootstrap_scene.material_load_failed", "SandboxMaterial.material could not be loaded from assets", {} });
            CaptureOperationResult(result);
            return false;
        }

        Rendering::MaterialLibrary::Instance().RegisterMaterial(m_SandboxMaterial->GetName(), m_SandboxMaterial);
        return true;
    }

    bool EditorLayer::BootstrapDemoScene() {
        if (!EnsureSandboxAssetsLoaded()) {
            m_WorkbenchReady = false;
            return false;
        }

        auto& operations = Application::GetInstance().GetOperations();
        const auto scenePath = m_ProjectContext.GetSceneRootPath() / "editor_workbench.scene";
        const auto packagedScenePath = std::filesystem::path("assets") / "SandboxScene.scene";

        auto tryLoadScene = [&](const std::filesystem::path& path, bool persistToWorkbench) -> bool {
            if (!std::filesystem::exists(path)) {
                return false;
            }

            Ref<Scene> loadedScene;
            CaptureOperationResult(operations.LoadScene(path, loadedScene));
            if (!m_LastOperationResult.Succeeded() || !loadedScene) {
                return false;
            }

            SetSceneContext(loadedScene);

            if (persistToWorkbench) {
                CaptureOperationResult(operations.SaveScene(*m_Scene, scenePath));
                if (!m_LastOperationResult.Succeeded()) {
                    return false;
                }
            }

            CaptureOperationResult(operations.ValidateScene(*m_Scene));
            m_WorkbenchReady = m_LastOperationResult.Succeeded();
            RefreshWorkbenchValidation();
            return m_WorkbenchReady;
        };

        if (tryLoadScene(scenePath, false)) {
            return true;
        }

        if (tryLoadScene(packagedScenePath, true)) {
            return true;
        }

        Ref<Scene> generatedScene;
        CaptureOperationResult(operations.CreateScene(m_Specification.InitialSceneName, generatedScene));
        if (!m_LastOperationResult.Succeeded() || !generatedScene) {
            m_WorkbenchReady = false;
            return false;
        }

        SetSceneContext(generatedScene);
        CreateSandboxEntities();

        CaptureOperationResult(operations.SaveScene(*m_Scene, scenePath));
        if (!m_LastOperationResult.Succeeded()) {
            m_WorkbenchReady = false;
            return false;
        }

        CaptureOperationResult(operations.ValidateScene(*m_Scene));
        m_WorkbenchReady = m_LastOperationResult.Succeeded();
        RefreshWorkbenchValidation();
        return m_WorkbenchReady;
    }

    void EditorLayer::CreateSandboxEntities() {
        auto firstMaterialInstance = m_SandboxMaterial->CreateInstance();
        firstMaterialInstance->SetParameter("u_Color", glm::vec4(1.0f, 1.0f, 0.8f, 1.0f));

        auto firstSquare = std::make_shared<Entity>(m_Scene->GetEntityManager().CreateEntity());
        firstSquare->AddComponent<MeshComponent>("Quad");
        firstSquare->AddComponent<MaterialComponent>(firstMaterialInstance);
        auto& firstTransform = firstSquare->GetComponent<TransformComponent>();
        firstTransform.Position.z -= 3.0f;
        firstTransform.Position += glm::vec3{ 0.5f, 0.5f, 0.0f };

        auto secondMaterialInstance = m_SandboxMaterial->CreateInstance();
        secondMaterialInstance->SetParameter("u_Color", glm::vec4(0.8f, 0.4f, 0.9f, 1.0f));
        secondMaterialInstance->SetParameter("u_TextureScale", glm::vec2(2.0f, 2.0f));

        auto secondSquare = std::make_shared<Entity>(m_Scene->GetEntityManager().CreateEntity());
        secondSquare->AddComponent<MeshComponent>("CustomSquare");
        secondSquare->AddComponent<MaterialComponent>(secondMaterialInstance);
        auto& secondTransform = secondSquare->GetComponent<TransformComponent>();
        secondTransform.Position.z -= 3.0f;
        secondTransform.Position -= glm::vec3{ 0.5f, 0.5f, 0.0f };

        auto sharedMaterialInstance = m_SandboxMaterial->CreateInstance();
        sharedMaterialInstance->SetParameter("u_Color", glm::vec4(0.8f, 0.0f, 0.9f, 1.0f));

        auto cubeEntity = std::make_shared<Entity>(m_Scene->GetEntityManager().CreateEntity());
        cubeEntity->AddComponent<MeshComponent>("Cube");
        cubeEntity->AddComponent<MaterialComponent>(sharedMaterialInstance);
        auto& cubeTransform = cubeEntity->GetComponent<TransformComponent>();
        cubeTransform.Position.z = -3.0f;
        cubeTransform.Position += glm::vec3{ -1.5f, 0.0f, 0.0f };
        cubeTransform.Scale *= 0.5f;

        auto sphereEntity = std::make_shared<Entity>(m_Scene->GetEntityManager().CreateEntity());
        sphereEntity->AddComponent<MeshComponent>("Sphere");
        sphereEntity->AddComponent<MaterialComponent>(sharedMaterialInstance);
        auto& sphereTransform = sphereEntity->GetComponent<TransformComponent>();
        sphereTransform.Position.z = -3.0f;
        sphereTransform.Position += glm::vec3{ 1.5f, 0.0f, 0.0f };
        sphereTransform.Scale *= 0.5f;
    }

    void EditorLayer::OnUpdate() {
        if (!m_WorkbenchReady || !m_Scene) {
            return;
        }

        m_EditorCamera->OnUpdate();
        const auto renderResult = Application::GetInstance().GetOperations().RenderSceneViewport(*m_Scene, *m_EditorCamera);
        if (!renderResult.Succeeded()) {
            CaptureOperationResult(renderResult);
            m_WorkbenchReady = false;
        }
        m_Scene->Update();
    }

    void EditorLayer::CaptureOperationResult(const ResultEnvelope& result) {
        m_LastOperationResult = result;
        m_WorkbenchState.CaptureResult(result, "editor.workbench");

        if (result.Succeeded()) {
            HE_CORE_INFO("[EditorWorkbench] {}", result.Summary);
            return;
        }

        HE_CORE_ERROR("[EditorWorkbench] {} ({})", result.Summary, result.Operation);
        for (const auto& detail : result.Details) {
            HE_CORE_ERROR("[EditorWorkbench] {} :: {}", detail.Code, detail.Message);
        }
    }

    void EditorLayer::RefreshWorkbenchValidation() {
        if (!m_Scene) {
            return;
        }

        auto& operations = Application::GetInstance().GetOperations();
        ValidationReport report;
        ApplicationValidationRequest request;
        request.Project = &m_ProjectContext;
        request.SceneTarget = m_Scene.get();
        request.ScriptScene = m_Scene.get();
        request.IncludeAssets = true;
        request.IncludeScripts = true;

        auto result = operations.Validate(request, &report);
        m_WorkbenchState.CaptureValidation(result, report, "editor.validation");
    }

    void EditorLayer::OnGuiRender() {
        OnDockingPanel();

        if (!m_WorkbenchReady) {
            ImGui::Begin("Workbench Status");
            ImGui::TextWrapped("%s", m_LastOperationResult.Summary.empty() ? "Editor workbench is not ready." : m_LastOperationResult.Summary.c_str());
            ImGui::Text("Operation: %s", m_LastOperationResult.Operation.c_str());
            ImGui::End();
            m_Concole->OnGuiRender();
            return;
        }

        OnScenePanel();
        if (m_SceneHierarchy) {
            m_SceneHierarchy->OnGuiRender();
        }
        m_Inspector->OnGuiRender();
        m_Concole->OnGuiRender();
        // ImGui::ShowDemoWindow();
    }

    void EditorLayer::OnDockingPanel() {
        // READ THIS !!!
        // TL;DR; this demo is more complicated than what most users you would normally use.
        // If we remove all options we are showcasing, this demo would become:
        //     void ShowExampleAppDockSpace()
        //     {
        //         ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
        //     }
        // In most cases you should be able to just call DockSpaceOverViewport() and ignore all the code below!
        // In this specific demo, we are not using DockSpaceOverViewport() because:
        // - (1) we allow the host window to be floating/moveable instead of filling the viewport (when opt_fullscreen == false)
        // - (2) we allow the host window to have padding (when opt_padding == true)
        // - (3) we expose many flags and need a way to have them visible.
        // - (4) we have a local menu bar in the host window (vs. you could use BeginMainMenuBar() + DockSpaceOverViewport()
        //      in your code, but we don't here because we allow the window to be floating)

        static bool enable_docking = true;

        static bool opt_fullscreen = true;
        static bool opt_padding = false;
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

        // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
        // because it would be confusing to have two docking targets within each others.
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        if (opt_fullscreen)
        {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
            window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        }
        else
        {
            dockspace_flags &= ~ImGuiDockNodeFlags_PassthruCentralNode;
        }

        // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background
        // and handle the pass-thru hole, so we ask Begin() to not render a background.
        if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
            window_flags |= ImGuiWindowFlags_NoBackground;

        // Important: note that we proceed even if Begin() returns false (aka window is collapsed).
        // This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
        // all active windows docked into it will lose their parent and become undocked.
        // We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
        // any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
        if (!opt_padding)
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("Hua Engine", &enable_docking, window_flags);
        if (!opt_padding)
            ImGui::PopStyleVar();

        if (opt_fullscreen)
            ImGui::PopStyleVar(2);

        // Submit the DockSpace
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
        {
            ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
            EnsureDefaultDockLayout(dockspace_id);
        }
        else
        {
            HE_ASSERT(false, "Docking is no enabled!");
        }

        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("Options"))
            {
                // Disabling fullscreen would allow the window to be moved to the front of other windows,
                // which we can't undo at the moment without finer window depth/z control.
                ImGui::MenuItem("Fullscreen", NULL, &opt_fullscreen);
                ImGui::MenuItem("Padding", NULL, &opt_padding);
                ImGui::Separator();

                if (ImGui::MenuItem("Flag: NoDockingOverCentralNode", "", (dockspace_flags & ImGuiDockNodeFlags_NoDockingOverCentralNode) != 0)) { dockspace_flags ^= ImGuiDockNodeFlags_NoDockingOverCentralNode; }
                if (ImGui::MenuItem("Flag: NoDockingSplit", "", (dockspace_flags & ImGuiDockNodeFlags_NoDockingSplit) != 0)) { dockspace_flags ^= ImGuiDockNodeFlags_NoDockingSplit; }
                if (ImGui::MenuItem("Flag: NoUndocking", "", (dockspace_flags & ImGuiDockNodeFlags_NoUndocking) != 0)) { dockspace_flags ^= ImGuiDockNodeFlags_NoUndocking; }
                if (ImGui::MenuItem("Flag: NoResize", "", (dockspace_flags & ImGuiDockNodeFlags_NoResize) != 0)) { dockspace_flags ^= ImGuiDockNodeFlags_NoResize; }
                if (ImGui::MenuItem("Flag: AutoHideTabBar", "", (dockspace_flags & ImGuiDockNodeFlags_AutoHideTabBar) != 0)) { dockspace_flags ^= ImGuiDockNodeFlags_AutoHideTabBar; }
                if (ImGui::MenuItem("Flag: PassthruCentralNode", "", (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode) != 0, opt_fullscreen)) { dockspace_flags ^= ImGuiDockNodeFlags_PassthruCentralNode; }
                ImGui::Separator();

                if (ImGui::MenuItem("Close", NULL, false, &enable_docking != NULL))
                    enable_docking = false;
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        ImGui::End();
    }

    void EditorLayer::EnsureDefaultDockLayout(ImGuiID dockspaceId) {
        if (m_DockLayoutInitialized) {
            return;
        }

        if (auto* node = ImGui::DockBuilderGetNode(dockspaceId); node && node->IsSplitNode()) {
            m_DockLayoutInitialized = true;
            return;
        }

        m_DockLayoutInitialized = true;

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->WorkSize);

        ImGuiID centerDockId = dockspaceId;
        ImGuiID leftDockId = ImGui::DockBuilderSplitNode(centerDockId, ImGuiDir_Left, 0.22f, nullptr, &centerDockId);
        ImGuiID rightDockId = ImGui::DockBuilderSplitNode(centerDockId, ImGuiDir_Right, 0.28f, nullptr, &centerDockId);
        ImGuiID bottomDockId = ImGui::DockBuilderSplitNode(centerDockId, ImGuiDir_Down, 0.28f, nullptr, &centerDockId);

        ImGui::DockBuilderDockWindow("Scene Hierarchy", leftDockId);
        ImGui::DockBuilderDockWindow("Inspector", rightDockId);
        ImGui::DockBuilderDockWindow("Console", bottomDockId);
        ImGui::DockBuilderDockWindow("Scene", centerDockId);
        ImGui::DockBuilderFinish(dockspaceId);
    }

    void EditorLayer::OnScenePanel() {
        if (!m_FrameBuffer) {
            return;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
        ImGui::Begin("Scene");
        ImVec2 scenePanelSize = ImGui::GetContentRegionAvail();
        if (scenePanelSize.x < 1.0f) scenePanelSize.x = 1.0f;
        if (scenePanelSize.y < 1.0f) scenePanelSize.y = 1.0f;

        const glm::vec2 newViewportSize = { scenePanelSize.x, scenePanelSize.y };
        if (newViewportSize != m_SceneViewportSize) {
            m_FrameBuffer->Resize((uint32_t)scenePanelSize.x, (uint32_t)scenePanelSize.y);
            m_SceneViewportSize = newViewportSize;
        }

        ImGui::Image(m_FrameBuffer->GetColorAttachment(),
            { m_SceneViewportSize.x , m_SceneViewportSize.y },
            { 0, 1 }, { 1, 0 });
        m_EditorCamera->SetViewport(m_SceneViewportSize.x, m_SceneViewportSize.y);
        ImGui::End();
        ImGui::PopStyleVar();
    }
}
