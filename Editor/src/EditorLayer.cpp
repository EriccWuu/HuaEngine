#include "enginepch.h"
#include "EditorLayer.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <system_error>

#include "HuaEngine/Application/ApplicationOperations.h"
#include "HuaEngine/Asset/AssetTypes.h"
#include "HuaEngine/Core/HostLaunch.h"
#include "HuaEngine/Core/ResourcePaths.h"
#include "HuaEngine/Rendering/RHI/RenderHardwareInterface.h"
#include "Interaction/EditorSceneCommands.h"
#include "ImGuizmo.h"
#include "imgui.h"
#include <imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>
#include "Module/Rendering/RenderingComponent.h"
#include "Selection.h"

namespace HE {
    namespace {
        const char* ToString(SceneDocumentSource source) {
            switch (source) {
                case SceneDocumentSource::NewScene: return "new";
                case SceneDocumentSource::LoadedFromDisk: return "loaded";
                default: return "unknown";
            }
        }

        void CopyToBuffer(std::string_view value, char* buffer, size_t size) {
            if (!buffer || size == 0) {
                return;
            }

            const auto copyLength = (std::min)(value.size(), size - 1);
            std::memcpy(buffer, value.data(), copyLength);
            buffer[copyLength] = '\0';
        }

        std::filesystem::path NormalizePath(const std::filesystem::path& path) {
            if (path.empty()) {
                return {};
            }

            std::error_code errorCode;
            auto absolutePath = std::filesystem::absolute(path, errorCode);
            if (errorCode) {
                return path.lexically_normal();
            }

            if (std::filesystem::exists(absolutePath, errorCode)) {
                auto canonicalPath = std::filesystem::weakly_canonical(absolutePath, errorCode);
                if (!errorCode) {
                    return canonicalPath;
                }
            }

            return absolutePath.lexically_normal();
        }

        std::string MakeSceneFileName(std::string_view sceneName) {
            std::string fileName = sceneName.empty() ? std::string("untitled_scene") : std::string(sceneName);
            for (char& character : fileName) {
                if (std::isalnum(static_cast<unsigned char>(character)) != 0) {
                    character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
                } else {
                    character = '_';
                }
            }

            if (!fileName.ends_with(".scene")) {
                fileName += ".scene";
            }

            return fileName;
        }

        std::string MeshNameFromGuid(const AssetGuid& guid) {
            if (guid == BuiltinAssetGuids::QuadMesh || guid == BuiltinAssetGuids::FallbackMesh) {
                return "Quad";
            }
            if (guid == BuiltinAssetGuids::CubeMesh) {
                return "Cube";
            }
            if (guid == BuiltinAssetGuids::SphereMesh) {
                return "Sphere";
            }
            return {};
        }

        Rendering::MeshComponent MakeBuiltinMeshComponent(const AssetGuid& guid) {
            Rendering::MeshComponent component;
            component.Mesh.Reference.Guid = guid;
            return component;
        }

        Rendering::MaterialComponent MakeBuiltinMaterialComponent() {
            Rendering::MaterialComponent component;
            component.Material.Reference.Guid = BuiltinAssetGuids::DefaultMaterial;
            return component;
        }

        bool CopyFileIfDifferent(const std::filesystem::path& sourcePath, const std::filesystem::path& destinationPath) {
            std::error_code errorCode;
            if (!std::filesystem::exists(sourcePath, errorCode) || !std::filesystem::is_regular_file(sourcePath, errorCode)) {
                return false;
            }

            const auto parentPath = destinationPath.parent_path();
            if (!parentPath.empty()) {
                std::filesystem::create_directories(parentPath, errorCode);
                if (errorCode) {
                    return false;
                }
            }

            std::filesystem::copy_file(sourcePath, destinationPath, std::filesystem::copy_options::overwrite_existing, errorCode);
            return !errorCode;
        }
    }

    EditorLayer::EditorLayer(EditorLayerSpecification specification)
        : Layer("EditorLayer"), m_Specification(std::move(specification)) {
        m_EditorCameraController = CreateRef<Editor::EditorCameraController>();
        m_ProjectPanel.reset(new ProjectPanel());
        m_ProjectPanel->SetWorkbenchState(&m_WorkbenchState);
        m_Inspector.reset(new InspectorPanel);
        m_Inspector->SetWorkbenchState(&m_WorkbenchState);
        m_Inspector->SetInteractionHost(&m_InteractionHost);
        m_Concole.reset(new ConcolePanel);
        m_Concole->SetWorkbenchState(&m_WorkbenchState);
        m_InteractionHost.SetStateChangedCallback([this]() {
            SyncSceneDocumentState();
            RefreshCommandInputs();
        });
    }

    void EditorLayer::OnAttach() {
        EnterProjectHub();
        InitializeStartupProjectSession();

        if (!m_Specification.StartupProjectPath.empty()) {
            if (OpenProjectFromPath(m_Specification.StartupProjectPath, false, {}, false) &&
                !m_Specification.StartupScenePath.empty()) {
                OpenSceneDocument(m_Specification.StartupScenePath);
            }
        }
    }

    bool EditorLayer::InitializeStartupProjectSession() {
        const char* localAppData = std::getenv("LOCALAPPDATA");
        if (localAppData && *localAppData) {
            m_WorkbenchRootPath = std::filesystem::path(localAppData) / "HuaEngine" / "Workbench";
        } else {
            m_WorkbenchRootPath = std::filesystem::temp_directory_path() / "huaengine_editor_workbench";
        }

        CopyToBuffer(m_WorkbenchRootPath.generic_string(), m_ProjectHubPathInput.data(), m_ProjectHubPathInput.size());
        CopyToBuffer(m_Specification.WorkbenchProjectName, m_ProjectHubNameInput.data(), m_ProjectHubNameInput.size());
        CopyToBuffer(m_Specification.InitialSceneName, m_NewSceneNameInput.data(), m_NewSceneNameInput.size());

        PersistedEditorSession persistedSession;
        if (EditorSessionStorage::Load(persistedSession) && persistedSession.HasProject()) {
            m_PersistedSession = std::move(persistedSession);
            m_HasPersistedSession = true;
            CopyToBuffer(m_PersistedSession.LastProjectRoot, m_ProjectHubPathInput.data(), m_ProjectHubPathInput.size());
            if (!m_PersistedSession.LastProjectName.empty()) {
                CopyToBuffer(m_PersistedSession.LastProjectName, m_ProjectHubNameInput.data(), m_ProjectHubNameInput.size());
            }

            RecordWorkbenchInfoEvent(
                "editor.workbench.restore_session",
                m_PersistedSession.LastProjectRoot,
                "Recovered the last editor session entry");
        } else {
            m_PersistedSession.Reset();
            m_HasPersistedSession = false;
        }

        RefreshCommandInputs();
        return true;
    }

    bool EditorLayer::OpenProjectFromPath(const std::filesystem::path& path, bool createIfMissing, std::string_view projectName, bool seedInitialScene) {
        auto& operations = Application::GetInstance().GetOperations();
        ProjectContext context;
        ProjectStatusReport status;

        const auto projectResult = createIfMissing
            ? operations.InitializeProject(path, &context, projectName)
            : operations.ResolveProjectContext(path, context);
        CaptureOperationResult(projectResult);
        if (!m_LastOperationResult.Succeeded()) {
            return false;
        }

        CaptureOperationResult(operations.CheckProjectStatus(context, &status));
        if (!m_LastOperationResult.Succeeded()) {
            return false;
        }

        return ActivateProjectSession(context, status, seedInitialScene);
    }

    bool EditorLayer::ActivateProjectSession(const ProjectContext& context, const ProjectStatusReport& status, bool seedInitialScene) {
        m_ProjectSession.Reset();
        m_SceneDocument.Reset();
        SetSceneContext(nullptr);
        SyncSceneDocumentState();
        RefreshInteractionHost();
        m_ProjectSession.Context = context;
        m_ProjectSession.LastStatus = status;
        m_ProjectSession.Loaded = true;

        if (m_HasPersistedSession && !m_PersistedSession.LastProjectRoot.empty()) {
            const auto persistedRoot = NormalizePath(m_PersistedSession.LastProjectRoot);
            if (persistedRoot == context.RootPath && !m_PersistedSession.LastScenePath.empty()) {
                m_ProjectSession.LastOpenedScenePath = NormalizePath(m_PersistedSession.LastScenePath);
            }
        }

        SyncWorkbenchSessionState();
        RefreshInteractionHost();
        RecordWorkbenchInfoEvent(
            "editor.workbench.project_session_ready",
            m_ProjectSession.Context.GetTargetId(),
            "Project session is ready");

        if (!InitializeWorkbenchShell()) {
            CloseProjectSession(true, "Workbench shell failed to initialize");
            return false;
        }

        bool restoredScene = false;
        if (!seedInitialScene) {
            restoredScene = RestoreLastSceneForSession();
        }

        if (!restoredScene && seedInitialScene) {
            if (m_Specification.BootstrapDemoScene) {
                if (!BootstrapDemoScene()) {
                    CloseProjectSession(true, "Bootstrap demo scene failed");
                    return false;
                }
            } else if (!CreateNewSceneDocument(m_Specification.InitialSceneName)) {
                CloseProjectSession(true, "Initial scene creation failed");
                return false;
            }
        }

        EnterWorkbenchShell();
        PersistCurrentProjectSession();
        return true;
    }

    bool EditorLayer::ResumePersistedProjectSession() {
        if (!m_HasPersistedSession || m_PersistedSession.LastProjectRoot.empty()) {
            auto result = ResultEnvelope::Failure("editor.workbench.resume_project", "editor.workbench", "No persisted project session is available");
            result.AddDetail({ DiagnosticSeverity::Warning, "editor.workbench.resume_project.missing", "Resume was requested but no persisted project session was found", {} });
            CaptureOperationResult(result);
            return false;
        }

        return OpenProjectFromPath(m_PersistedSession.LastProjectRoot, false, {}, false);
    }

    void EditorLayer::SetSceneContext(const Ref<Scene>& scene) {
        Selection::ClearSelection();
        if (!m_HierarchyPanel) {
            m_HierarchyPanel.reset(new HierarchyPanel(scene));
            m_HierarchyPanel->SetWorkbenchState(&m_WorkbenchState);
            m_HierarchyPanel->SetInteractionHost(&m_InteractionHost);
        } else {
            m_HierarchyPanel->SetContext(scene);
            m_HierarchyPanel->SetInteractionHost(&m_InteractionHost);
        }
    }

    void EditorLayer::SetSceneDocument(const Ref<Scene>& scene, const std::filesystem::path& scenePath, SceneDocumentSource source) {
        m_SceneDocument.SceneRef = scene;
        m_SceneDocument.ScenePath = scenePath;
        m_SceneDocument.Source = source;
        m_SceneDocument.Dirty = false;
        m_SceneDocument.DisplayName = !scenePath.empty()
            ? scenePath.stem().string()
            : (scene ? scene->GetName() : m_Specification.InitialSceneName);
        SetSceneContext(scene);
        SyncSceneDocumentState();
        RefreshInteractionHost();
        m_InteractionHost.ResetCommandHistory(true);
        RefreshCommandInputs();
        if (m_ProjectPanel) {
            m_ProjectPanel->SetCurrentScenePath(scenePath);
        }
        RecordWorkbenchInfoEvent(
            "editor.workbench.scene_document_ready",
            scenePath.empty() ? "scene:new" : scenePath.generic_string(),
            "Scene document is ready");
        if (m_RenderTarget) {
            if (!BindSceneDocumentToShell()) {
                m_WorkbenchReady = false;
            }
        }
    }

    void EditorLayer::EnterProjectHub() {
        Selection::ClearSelection();
        m_Mode = EditorWorkbenchMode::ProjectHub;
        m_WorkbenchReady = false;
        m_InteractionHost.Reset();
        m_RenderTarget.reset();
        m_SceneViewportSize = { 0.0f, 0.0f };
        if (m_ProjectPanel) {
            m_ProjectPanel->SetProjectRoot({});
            m_ProjectPanel->SetCurrentScenePath({});
        }
        RecordWorkbenchInfoEvent("editor.workbench.project_hub", "editor.workbench", "Workbench switched to project-entry mode");
    }

    void EditorLayer::EnterWorkbenchShell() {
        m_Mode = EditorWorkbenchMode::WorkbenchShell;
        m_WorkbenchReady = true;
        SyncWorkbenchSessionState();
        SyncSceneDocumentState();
        RefreshInteractionHost();
        RefreshCommandInputs();
        if (m_ProjectPanel) {
            m_ProjectPanel->SetProjectRoot(m_ProjectSession.Context.RootPath);
            m_ProjectPanel->SetCurrentScenePath(m_SceneDocument.ScenePath);
        }
    }

    void EditorLayer::CloseProjectSession(bool preserveResumeState, std::string_view summary) {
        if (preserveResumeState && m_ProjectSession.IsLoaded()) {
            PersistCurrentProjectSession();
        } else if (!preserveResumeState) {
            ClearPersistedProjectSession();
        }

        m_ProjectSession.Reset();
        m_SceneDocument.Reset();
        SetSceneContext(nullptr);
        SyncWorkbenchSessionState();
        SyncSceneDocumentState();
        RefreshInteractionHost();
        RefreshCommandInputs();
        EnterProjectHub();

        if (m_HasPersistedSession && preserveResumeState) {
            CopyToBuffer(m_PersistedSession.LastProjectRoot, m_ProjectHubPathInput.data(), m_ProjectHubPathInput.size());
            CopyToBuffer(
                m_PersistedSession.LastProjectName.empty() ? m_Specification.WorkbenchProjectName : m_PersistedSession.LastProjectName,
                m_ProjectHubNameInput.data(),
                m_ProjectHubNameInput.size());
        } else {
            CopyToBuffer(m_WorkbenchRootPath.generic_string(), m_ProjectHubPathInput.data(), m_ProjectHubPathInput.size());
            CopyToBuffer(m_Specification.WorkbenchProjectName, m_ProjectHubNameInput.data(), m_ProjectHubNameInput.size());
        }

        RecordWorkbenchInfoEvent("editor.workbench.close_project", "editor.workbench", std::string(summary));
    }

    void EditorLayer::SyncWorkbenchSessionState() {
        if (!m_ProjectSession.IsLoaded()) {
            m_WorkbenchState.ClearProjectSessionSummary();
            return;
        }

        ProjectSessionSummary summary;
        summary.Loaded = true;
        summary.Operational = m_ProjectSession.LastStatus.IsOperational();
        summary.ProjectName = m_ProjectSession.GetDisplayName();
        summary.RootPath = m_ProjectSession.Context.RootPath.generic_string();
        summary.ProjectFilePath = m_ProjectSession.Context.ProjectFilePath.generic_string();
        summary.LastOpenedScenePath = m_ProjectSession.LastOpenedScenePath.generic_string();
        m_WorkbenchState.SetProjectSessionSummary(summary);
    }

    void EditorLayer::SyncSceneDocumentState() {
        if (!m_SceneDocument.IsLoaded()) {
            m_WorkbenchState.ClearSceneDocumentSummary();
            return;
        }

        SceneDocumentSummary summary;
        summary.Loaded = true;
        summary.Dirty = m_SceneDocument.Dirty;
        summary.DisplayName = m_SceneDocument.DisplayName;
        summary.ScenePath = m_SceneDocument.ScenePath.generic_string();
        summary.Source = ToString(m_SceneDocument.Source);
        if (m_SceneDocument.SceneRef) {
            summary.EntityCount = static_cast<uint32_t>(m_SceneDocument.SceneRef->GetWorld().GetEntityCount());
        }
        m_WorkbenchState.SetSceneDocumentSummary(summary);
    }


    void EditorLayer::RefreshInteractionHost() {
        m_InteractionHost.Bind(&m_WorkbenchState, &m_ProjectSession, &m_SceneDocument);
        m_InteractionHost.ContextMenus().Clear();
        m_InteractionHost.Shortcuts().Clear();
        m_InteractionHost.DragDrop().Clear();

        m_InteractionHost.ContextMenus().Replace("hierarchy.window", {
            {
                .Id = "entity.create",
                .Label = "New Entity",
                .Shortcut = "Ctrl+Shift+N",
                .Tooltip = "Create a new entity in the active scene",
                .Enabled = true,
                .IsEnabled = [this]() { return m_InteractionHost.HasActiveScene(); },
                .Trigger = [this]() { CreateEntityFromHierarchy(); }
            }
        });
        m_InteractionHost.ContextMenus().Replace("hierarchy.entity", {
            {
                .Id = "entity.create",
                .Label = "New Entity",
                .Shortcut = "Ctrl+Shift+N",
                .Tooltip = "Create a new entity in the active scene",
                .Enabled = true,
                .IsEnabled = [this]() { return m_InteractionHost.HasActiveScene(); },
                .Trigger = [this]() { CreateEntityFromHierarchy(); }
            },
            {
                .Id = "entity.delete",
                .Label = "Delete Selected",
                .Shortcut = "Del",
                .Tooltip = "Delete the current selection and support undo/redo",
                .Enabled = true,
                .IsEnabled = []() { return Selection::HasSelection(); },
                .Trigger = [this]() { DeleteSelectedEntities(); }
            }
        });

        std::vector<ContextMenuActionDescriptor> inspectorActions;
        for (const auto& descriptor : GetEditorInspectableComponents()) {
            inspectorActions.push_back({
                .Id = descriptor.Id + ".remove",
                .Label = "Remove " + descriptor.DisplayName + " Component",
                .Shortcut = "",
                .Tooltip = "Remove a component from the selected entity",
                .Enabled = true,
                .IsEnabled = [this, type = descriptor.Type]() {
                    if (!m_SceneDocument.SceneRef || !Selection::HasSingleSelection()) {
                        return false;
                    }

                    return CanRemoveInspectableComponent(type, Selection::ResolvePrimarySelection(m_SceneDocument.SceneRef->GetWorld()));
                },
                .Trigger = [this, type = descriptor.Type]() { RemoveComponentFromPrimarySelection(type); }
            });
        }
        m_InteractionHost.ContextMenus().Replace("inspector.window", {});
        m_InteractionHost.ContextMenus().Replace("inspector.entity", inspectorActions);

        m_InteractionHost.DragDrop().Register({
            .Id = "hierarchy.entity.reorder",
            .Label = "Hierarchy Entity",
            .PayloadType = "HE_HIERARCHY_ENTITY",
            .Source = "hierarchy.entity",
            .Target = "hierarchy.entity",
            .Enabled = true
        });

        m_InteractionHost.Shortcuts().Register({
            .CommandId = "editor.undo",
            .DisplayName = "Undo",
            .Chord = ImGuiMod_Ctrl | ImGuiKey_Z,
            .Shortcut = "Ctrl+Z",
            .IsEnabled = [this]() { return m_InteractionHost.CanUndo(); },
            .Trigger = [this]() { ExecuteUndo(); }
        });
        m_InteractionHost.Shortcuts().Register({
            .CommandId = "editor.redo",
            .DisplayName = "Redo",
            .Chord = ImGuiMod_Ctrl | ImGuiKey_Y,
            .Shortcut = "Ctrl+Y",
            .IsEnabled = [this]() { return m_InteractionHost.CanRedo(); },
            .Trigger = [this]() { ExecuteRedo(); }
        });
        m_InteractionHost.Shortcuts().Register({
            .CommandId = "editor.entity.create",
            .DisplayName = "Create Entity",
            .Chord = ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_N,
            .Shortcut = "Ctrl+Shift+N",
            .IsEnabled = [this]() { return m_InteractionHost.HasActiveScene(); },
            .Trigger = [this]() { CreateEntityFromHierarchy(); }
        });
        m_InteractionHost.Shortcuts().Register({
            .CommandId = "editor.entity.delete",
            .DisplayName = "Delete Selected",
            .Chord = ImGuiKey_Delete,
            .Shortcut = "Del",
            .IsEnabled = []() { return Selection::HasSelection(); },
            .Trigger = [this]() { DeleteSelectedEntities(); }
        });
        m_InteractionHost.Shortcuts().Register({
            .CommandId = "editor.scene.save",
            .DisplayName = "Save Scene",
            .Chord = ImGuiMod_Ctrl | ImGuiKey_S,
            .Shortcut = "Ctrl+S",
            .IsEnabled = [this]() { return m_SceneDocument.IsLoaded(); },
            .Trigger = [this]() { SaveActiveSceneDocument(); }
        });

        m_Inspector->SetInteractionHost(&m_InteractionHost);
        m_Inspector->SetAddComponentCallback([this](EditorInspectableComponent type) {
            AddComponentToPrimarySelection(type);
        });
        m_Inspector->SetRemoveComponentCallback([this](EditorInspectableComponent type) {
            RemoveComponentFromPrimarySelection(type);
        });
        if (m_HierarchyPanel) {
            m_HierarchyPanel->SetInteractionHost(&m_InteractionHost);
        }
    }


    void EditorLayer::RefreshCommandInputs() {
        CopyToBuffer(m_Specification.InitialSceneName, m_NewSceneNameInput.data(), m_NewSceneNameInput.size());

        if (!m_ProjectSession.IsLoaded()) {
            m_SceneOpenPathInput[0] = '\0';
            m_SceneSaveAsPathInput[0] = '\0';
            return;
        }

        const auto sceneRoot = m_ProjectSession.Context.GetAssetRootPath();
        const auto defaultOpenPath = !m_SceneDocument.ScenePath.empty() ? m_SceneDocument.ScenePath : sceneRoot;
        const auto defaultSaveAsPath = !m_SceneDocument.ScenePath.empty()
            ? m_SceneDocument.ScenePath
            : (sceneRoot / MakeSceneFileName(m_SceneDocument.DisplayName.empty() ? m_Specification.InitialSceneName : m_SceneDocument.DisplayName));

        CopyToBuffer(defaultOpenPath.generic_string(), m_SceneOpenPathInput.data(), m_SceneOpenPathInput.size());
        CopyToBuffer(defaultSaveAsPath.generic_string(), m_SceneSaveAsPathInput.data(), m_SceneSaveAsPathInput.size());
        if (m_SceneDocument.IsLoaded() && !m_SceneDocument.DisplayName.empty()) {
            CopyToBuffer(m_SceneDocument.DisplayName, m_NewSceneNameInput.data(), m_NewSceneNameInput.size());
        }
    }

    std::filesystem::path EditorLayer::ResolveScenePathInput(const std::filesystem::path& scenePath) const {
        if (scenePath.empty()) {
            return {};
        }

        if (scenePath.is_absolute() || !m_ProjectSession.IsLoaded()) {
            return NormalizePath(scenePath);
        }

        return NormalizePath(m_ProjectSession.Context.GetAssetRootPath() / scenePath);
    }

    void EditorLayer::PersistCurrentProjectSession() {
        if (!m_ProjectSession.IsLoaded()) {
            return;
        }

        m_PersistedSession.LastProjectRoot = m_ProjectSession.Context.RootPath.generic_string();
        m_PersistedSession.LastProjectName = m_ProjectSession.GetDisplayName();
        m_PersistedSession.LastScenePath = m_ProjectSession.LastOpenedScenePath.generic_string();
        m_HasPersistedSession = true;
        if (!EditorSessionStorage::Save(m_PersistedSession)) {
            HE_CORE_WARN("[EditorWorkbench] Failed to persist editor session to disk");
        }
    }

    void EditorLayer::ClearPersistedProjectSession() {
        m_PersistedSession.Reset();
        m_HasPersistedSession = false;
        EditorSessionStorage::Clear();
    }

    bool EditorLayer::InitializeWorkbenchShell() {
        RenderTargetSpecification spec;
        spec.Width = 1280;
        spec.Height = 720;
        spec.Attachments = { RenderTargetTextureFormat::RGBA8, RenderTargetTextureFormat::DEPTH24_STENCIL8 };
        m_RenderTarget = Rendering::RenderHardwareInterface::GetDevice().CreateRenderTarget({ .Specification = spec });

        if (m_SceneDocument.SceneRef) {
            return BindSceneDocumentToShell();
        }

        RecordWorkbenchInfoEvent("editor.workbench.initialize_shell", "editor.workbench", "Workbench shell is ready without an active scene document");
        return true;
    }

    bool EditorLayer::BindSceneDocumentToShell() {
        if (!m_SceneDocument.SceneRef || !m_RenderTarget) {
            return true;
        }

        auto& operations = Application::GetInstance().GetOperations();
        CaptureOperationResult(operations.AttachSceneViewportRenderer(m_SceneDocument.SceneRef, m_RenderTarget));
        if (!m_LastOperationResult.Succeeded()) {
            return false;
        }

        RefreshWorkbenchValidation();
        return true;
    }

    bool EditorLayer::RestoreLastSceneForSession() {
        if (m_ProjectSession.LastOpenedScenePath.empty()) {
            return false;
        }

        const auto scenePath = NormalizePath(m_ProjectSession.LastOpenedScenePath);
        std::error_code errorCode;
        if (!std::filesystem::exists(scenePath, errorCode) || !std::filesystem::is_regular_file(scenePath, errorCode)) {
            m_ProjectSession.LastOpenedScenePath.clear();
            SyncWorkbenchSessionState();
            PersistCurrentProjectSession();
            RecordWorkbenchInfoEvent(
                "editor.workbench.restore_scene_skipped",
                scenePath.generic_string(),
                "Last scene record was missing and was cleared from the project session");
            return false;
        }

        if (!OpenSceneDocument(scenePath)) {
            m_ProjectSession.LastOpenedScenePath.clear();
            SyncWorkbenchSessionState();
            PersistCurrentProjectSession();
            return false;
        }

        return true;
    }

    bool EditorLayer::EnsureSandboxAssetsLoaded() {
        MeshManager::Instance().LoadDefaultMeshes();

        const auto customMeshPath = ResourcePaths::ResolveEngineResourcePath("CustomMesh.mesh");
        auto customMesh = Mesh::LoadFromFile(customMeshPath.generic_string());
        if (!customMesh) {
            auto result = ResultEnvelope::Failure("editor.workbench.bootstrap_scene", customMeshPath.generic_string(), "Failed to load custom sandbox mesh");
            result.AddDetail({ DiagnosticSeverity::Error, "editor.workbench.bootstrap_scene.mesh_load_failed", "CustomMesh.mesh could not be loaded from Resources", {} });
            CaptureOperationResult(result);
            return false;
        }
        MeshManager::Instance().RegisterMesh("CustomSquare", customMesh);

        m_SandboxMaterial = Material::Create("SandboxMaterial");
        const auto materialPath = ResourcePaths::ResolveEngineResourcePath("SandboxMaterial.material");
        if (!Serialization::LoadMaterial(materialPath.generic_string(), *m_SandboxMaterial)) {
            auto result = ResultEnvelope::Failure("editor.workbench.bootstrap_scene", materialPath.generic_string(), "Failed to load sandbox material");
            result.AddDetail({ DiagnosticSeverity::Error, "editor.workbench.bootstrap_scene.material_load_failed", "SandboxMaterial.material could not be loaded from Resources", {} });
            CaptureOperationResult(result);
            return false;
        }

        Rendering::MaterialLibrary::Instance().RegisterMaterial(m_SandboxMaterial->GetName(), m_SandboxMaterial);
        return true;
    }

    bool EditorLayer::SeedDemoProjectAssets() {
        if (!m_ProjectSession.IsLoaded()) {
            return false;
        }

        const auto assetRoot = m_ProjectSession.Context.GetAssetRootPath();
        const auto engineMeshPath = ResourcePaths::ResolveEngineResourcePath("CustomMesh.mesh");
        const auto engineMaterialPath = ResourcePaths::ResolveEngineResourcePath("SandboxMaterial.material");
        const auto engineShaderPath = ResourcePaths::ResolveEngineResourcePath(std::filesystem::path("shaders") / "sandbox.glsl");
        const bool customMeshCopied = CopyFileIfDifferent(engineMeshPath, assetRoot / "CustomSquare.mesh");
        const bool materialCopied = CopyFileIfDifferent(engineMaterialPath, assetRoot / "SandboxMaterial.material");
        const bool shaderCopied = CopyFileIfDifferent(engineShaderPath, assetRoot / "shaders" / "sandbox.glsl");

        if (!customMeshCopied || !materialCopied || !shaderCopied) {
            auto result = ResultEnvelope::Failure(
                "editor.workbench.seed_demo_assets",
                assetRoot.generic_string(),
                "Failed to seed one or more demo assets into the project asset root");
            result.AddDetail({ DiagnosticSeverity::Error, "editor.workbench.seed_demo_assets.copy_failed", "Expected demo assets could not be copied into the project", assetRoot.generic_string() });
            CaptureOperationResult(result);
            return false;
        }

        RecordWorkbenchInfoEvent(
            "editor.workbench.seed_demo_assets",
            assetRoot.generic_string(),
            "Seeded demo mesh and material into the project asset root");
        return true;
    }

    bool EditorLayer::EnsureMeshAvailable(std::string_view meshName) {
        if (meshName.empty()) {
            return true;
        }

        if (meshName == "Quad" || meshName == "Cube" || meshName == "Sphere") {
            MeshManager::Instance().LoadDefaultMeshes();
            return MeshManager::Instance().IsMeshLoaded(std::string(meshName));
        }

        if (MeshManager::Instance().IsMeshLoaded(std::string(meshName))) {
            return true;
        }

        std::filesystem::path projectMeshPath;
        if (m_ProjectSession.IsLoaded()) {
            projectMeshPath = m_ProjectSession.Context.GetAssetRootPath() / (std::string(meshName) + ".mesh");
        }

        auto loadMeshFromPath = [&](const std::filesystem::path& path, bool mirrorIntoProject) -> bool {
            std::error_code errorCode;
            if (path.empty() || !std::filesystem::exists(path, errorCode) || !std::filesystem::is_regular_file(path, errorCode)) {
                return false;
            }

            auto mesh = Mesh::LoadFromFile(path.generic_string());
            if (!mesh) {
                return false;
            }

            MeshManager::Instance().RegisterMesh(std::string(meshName), mesh);
            if (!mesh->GetName().empty() && mesh->GetName() != meshName) {
                MeshManager::Instance().RegisterMesh(mesh->GetName(), mesh);
            }

            if (mirrorIntoProject && m_ProjectSession.IsLoaded() && !projectMeshPath.empty()) {
                CopyFileIfDifferent(path, projectMeshPath);
            }

            return true;
        };

        if (loadMeshFromPath(projectMeshPath, false)) {
            return true;
        }

        if (meshName == "CustomSquare") {
            if (loadMeshFromPath(ResourcePaths::ResolveEngineResourcePath("CustomMesh.mesh"), true)) {
                return true;
            }
        }

        auto result = ResultEnvelope::Failure(
            "editor.workbench.ensure_mesh_available",
            std::string(meshName),
            "Required scene mesh could not be loaded");
        result.AddDetail({ DiagnosticSeverity::Error, "editor.workbench.ensure_mesh_available.missing", "Mesh asset was not available in the project or runtime demo assets", std::string(meshName) });
        CaptureOperationResult(result);
        return false;
    }

    bool EditorLayer::EnsureMaterialAvailable(std::string_view materialName) {
        if (materialName.empty()) {
            return true;
        }

        if (Rendering::MaterialLibrary::Instance().HasMaterial(std::string(materialName))) {
            auto cachedMaterial = Rendering::MaterialLibrary::Instance().GetMaterial(std::string(materialName));
            if (cachedMaterial && cachedMaterial->GetShaderProgram()) {
                return true;
            }
        }

        std::filesystem::path projectMaterialPath;
        std::filesystem::path projectShaderPath;
        if (m_ProjectSession.IsLoaded()) {
            projectMaterialPath = m_ProjectSession.Context.GetAssetRootPath() / (std::string(materialName) + ".material");
            projectShaderPath = m_ProjectSession.Context.GetAssetRootPath() / "shaders" / "sandbox.glsl";
        }

        auto loadMaterialFromPath = [&](const std::filesystem::path& path, bool mirrorIntoProject) -> bool {
            std::error_code errorCode;
            if (path.empty() || !std::filesystem::exists(path, errorCode) || !std::filesystem::is_regular_file(path, errorCode)) {
                return false;
            }

            auto material = Material::Create(std::string(materialName));
            if (!Serialization::LoadMaterial(path.generic_string(), *material)) {
                return false;
            }

            if (!material->GetShaderProgram()) {
                return false;
            }

            Rendering::MaterialLibrary::Instance().RegisterMaterial(std::string(materialName), material);
            if (!material->GetName().empty() && material->GetName() != materialName) {
                Rendering::MaterialLibrary::Instance().RegisterMaterial(material->GetName(), material);
            }

            if (material->GetName() == "SandboxMaterial") {
                m_SandboxMaterial = material;
            }

            if (mirrorIntoProject && m_ProjectSession.IsLoaded() && !projectMaterialPath.empty()) {
                CopyFileIfDifferent(path, projectMaterialPath);
                if (!projectShaderPath.empty()) {
                    CopyFileIfDifferent(ResourcePaths::ResolveEngineResourcePath(std::filesystem::path("shaders") / "sandbox.glsl"), projectShaderPath);
                }
            }

            return true;
        };

        if (materialName == "SandboxMaterial" && !projectShaderPath.empty()) {
            CopyFileIfDifferent(ResourcePaths::ResolveEngineResourcePath(std::filesystem::path("shaders") / "sandbox.glsl"), projectShaderPath);
        }

        if (loadMaterialFromPath(projectMaterialPath, false)) {
            return true;
        }

        if (materialName == "SandboxMaterial") {
            if (loadMaterialFromPath(ResourcePaths::ResolveEngineResourcePath("SandboxMaterial.material"), true)) {
                return true;
            }
        }

        auto result = ResultEnvelope::Failure(
            "editor.workbench.ensure_material_available",
            std::string(materialName),
            "Required scene material could not be loaded");
        result.AddDetail({ DiagnosticSeverity::Error, "editor.workbench.ensure_material_available.missing", "Material asset was not available in the project or runtime demo assets", std::string(materialName) });
        CaptureOperationResult(result);
        return false;
    }

    bool EditorLayer::WarmupSceneAssets(const Ref<Scene>& scene) {
        if (!scene) {
            return true;
        }

        MeshManager::Instance().LoadDefaultMeshes();
        bool meshesAvailable = true;
        scene->GetWorld().Query<Rendering::MeshComponent>().ForEach([&](Entity, Rendering::MeshComponent& meshComponent) {
            const std::string meshName = MeshNameFromGuid(meshComponent.Mesh.Reference.Guid);
            if (!meshName.empty() && !EnsureMeshAvailable(meshName)) {
                meshesAvailable = false;
                return;
            }
        });

        bool materialsAvailable = true;
        scene->GetWorld().Query<Rendering::MaterialComponent>().ForEach([&](Entity, Rendering::MaterialComponent& materialComponent) {
			const auto& materialGuid = materialComponent.Material.Reference.Guid;
			if (materialGuid == BuiltinAssetGuids::DefaultMaterial || materialGuid == BuiltinAssetGuids::FallbackMaterial) {
				if (!Rendering::MaterialLibrary::Instance().GetDefaultMaterial()) {
					Rendering::MaterialLibrary::Instance().CreateDefaultMaterials();
				}
				return;
			}

			// Non-builtin material GUID resolution is handled by the asset resolver integration phase.
        });

        return meshesAvailable && materialsAvailable;
    }

    bool EditorLayer::BootstrapDemoScene() {
        if (!EnsureSandboxAssetsLoaded()) {
            m_WorkbenchReady = false;
            return false;
        }

        auto& operations = Application::GetInstance().GetOperations();
        const auto scenePath = m_ProjectSession.Context.GetAssetRootPath() / "editor_workbench.scene";
        const auto packagedScenePath = ResourcePaths::ResolveEngineResourcePath("SandboxScene.scene");

        auto tryLoadScene = [&](const std::filesystem::path& path, bool persistToWorkbench) -> bool {
            if (!std::filesystem::exists(path)) {
                return false;
            }

            Ref<Scene> loadedScene;
            CaptureOperationResult(operations.LoadScene(path, loadedScene));
            if (!m_LastOperationResult.Succeeded() || !loadedScene) {
                return false;
            }

            if (!WarmupSceneAssets(loadedScene)) {
                return false;
            }

            SetSceneDocument(loadedScene, path, SceneDocumentSource::LoadedFromDisk);
            m_ProjectSession.LastOpenedScenePath = path;
            SyncWorkbenchSessionState();

            if (persistToWorkbench) {
                if (!SeedDemoProjectAssets()) {
                    return false;
                }
                CaptureOperationResult(operations.SaveScene(*m_SceneDocument.SceneRef, scenePath));
                if (!m_LastOperationResult.Succeeded()) {
                    return false;
                }
                m_SceneDocument.MarkSaved(scenePath);
                m_InteractionHost.MarkSaved();
                m_ProjectSession.LastOpenedScenePath = scenePath;
                SyncSceneDocumentState();
                SyncWorkbenchSessionState();
            }

            CaptureOperationResult(operations.ValidateScene(*m_SceneDocument.SceneRef));
            m_WorkbenchReady = m_LastOperationResult.Succeeded();
            RefreshWorkbenchValidation();
            PersistCurrentProjectSession();
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

        SetSceneDocument(generatedScene, scenePath, SceneDocumentSource::NewScene);
        CreateSandboxEntities();
        if (!SeedDemoProjectAssets()) {
            m_WorkbenchReady = false;
            return false;
        }

        CaptureOperationResult(operations.SaveScene(*m_SceneDocument.SceneRef, scenePath));
        if (!m_LastOperationResult.Succeeded()) {
            m_WorkbenchReady = false;
            return false;
        }

        m_SceneDocument.MarkSaved(scenePath);
        m_InteractionHost.MarkSaved();
        m_ProjectSession.LastOpenedScenePath = scenePath;
        SyncSceneDocumentState();
        SyncWorkbenchSessionState();
        CaptureOperationResult(operations.ValidateScene(*m_SceneDocument.SceneRef));
        m_WorkbenchReady = m_LastOperationResult.Succeeded();
        RefreshWorkbenchValidation();
        PersistCurrentProjectSession();
        return m_WorkbenchReady;
    }

    void EditorLayer::CreateSandboxEntities() {
        auto scene = m_SceneDocument.SceneRef;
        if (!scene) {
            return;
        }

        auto firstSquare = scene->GetWorld().CreateEntity("First Square");
        firstSquare.AddComponent<TransformComponent>();
        firstSquare.AddComponent<MeshComponent>(MakeBuiltinMeshComponent(BuiltinAssetGuids::QuadMesh));
        auto& firstMaterial = firstSquare.AddComponent<MaterialComponent>(MakeBuiltinMaterialComponent());
        firstMaterial.Overrides.SetVec4("u_Color", glm::vec4(1.0f, 1.0f, 0.8f, 1.0f));
        auto& firstTransform = firstSquare.GetComponent<TransformComponent>();
        firstTransform.Position.z -= 3.0f;
        firstTransform.Position += glm::vec3{ 0.5f, 0.5f, 0.0f };

        auto secondSquare = scene->GetWorld().CreateEntity("Second Square");
        secondSquare.AddComponent<TransformComponent>();
        secondSquare.AddComponent<MeshComponent>(MakeBuiltinMeshComponent(BuiltinAssetGuids::QuadMesh));
        auto& secondMaterial = secondSquare.AddComponent<MaterialComponent>(MakeBuiltinMaterialComponent());
        secondMaterial.Overrides.SetVec4("u_Color", glm::vec4(0.8f, 0.4f, 0.9f, 1.0f));
        secondMaterial.Overrides.Parameters["u_TextureScale"] = glm::vec2(2.0f, 2.0f);
        auto& secondTransform = secondSquare.GetComponent<TransformComponent>();
        secondTransform.Position.z -= 3.0f;
        secondTransform.Position -= glm::vec3{ 0.5f, 0.5f, 0.0f };

        auto cubeEntity = scene->GetWorld().CreateEntity("Cube");
        cubeEntity.AddComponent<TransformComponent>();
        cubeEntity.AddComponent<MeshComponent>(MakeBuiltinMeshComponent(BuiltinAssetGuids::CubeMesh));
        auto& cubeMaterial = cubeEntity.AddComponent<MaterialComponent>(MakeBuiltinMaterialComponent());
        cubeMaterial.Overrides.SetVec4("u_Color", glm::vec4(0.8f, 0.0f, 0.9f, 1.0f));
        auto& cubeTransform = cubeEntity.GetComponent<TransformComponent>();
        cubeTransform.Position.z = -3.0f;
        cubeTransform.Position += glm::vec3{ -1.5f, 0.0f, 0.0f };
        cubeTransform.Scale *= 0.5f;

        auto sphereEntity = scene->GetWorld().CreateEntity("Sphere");
        sphereEntity.AddComponent<TransformComponent>();
        sphereEntity.AddComponent<MeshComponent>(MakeBuiltinMeshComponent(BuiltinAssetGuids::SphereMesh));
        auto& sphereMaterial = sphereEntity.AddComponent<MaterialComponent>(MakeBuiltinMaterialComponent());
        sphereMaterial.Overrides.SetVec4("u_Color", glm::vec4(0.8f, 0.0f, 0.9f, 1.0f));
        auto& sphereTransform = sphereEntity.GetComponent<TransformComponent>();
        sphereTransform.Position.z = -3.0f;
        sphereTransform.Position += glm::vec3{ 1.5f, 0.0f, 0.0f };
        sphereTransform.Scale *= 0.5f;
        CaptureOperationResult(m_InteractionHost.MarkExternalSceneMutation(
            "editor.scene.seed_demo_entities",
            m_SceneDocument.ScenePath.empty() ? "scene:new" : m_SceneDocument.ScenePath.generic_string(),
            "Seeded the default demo entities into the active scene document"));
    }

    bool EditorLayer::CreateNewSceneDocument(std::string_view sceneName) {
        if (!m_ProjectSession.IsLoaded()) {
            auto result = ResultEnvelope::Failure("editor.workbench.new_scene", "editor.workbench", "A project session is required before creating a scene");
            result.AddDetail({ DiagnosticSeverity::Error, "editor.workbench.new_scene.project_missing", "Open or create a project before creating a scene document", {} });
            CaptureOperationResult(result);
            return false;
        }

        Ref<Scene> scene;
        CaptureOperationResult(Application::GetInstance().GetOperations().CreateScene(sceneName, scene));
        if (!m_LastOperationResult.Succeeded() || !scene) {
            return false;
        }

        SetSceneDocument(scene, {}, SceneDocumentSource::NewScene);
        m_SceneDocument.DisplayName = sceneName.empty() ? m_Specification.InitialSceneName : std::string(sceneName);
        m_ProjectSession.LastOpenedScenePath.clear();
        SyncSceneDocumentState();
        SyncWorkbenchSessionState();
        CaptureOperationResult(m_InteractionHost.MarkExternalSceneMutation(
            "editor.scene.create_new_document",
            "scene:new",
            "Created a new unsaved scene document"));
        RefreshWorkbenchValidation();
        PersistCurrentProjectSession();
        return true;
    }

    bool EditorLayer::OpenSceneDocument(const std::filesystem::path& scenePath) {
        if (!m_ProjectSession.IsLoaded()) {
            auto result = ResultEnvelope::Failure("editor.workbench.open_scene", "editor.workbench", "A project session is required before opening a scene");
            result.AddDetail({ DiagnosticSeverity::Error, "editor.workbench.open_scene.project_missing", "Open or create a project before loading a scene document", {} });
            CaptureOperationResult(result);
            return false;
        }

        const auto resolvedPath = ResolveScenePathInput(scenePath);
        Ref<Scene> scene;
        CaptureOperationResult(Application::GetInstance().GetOperations().LoadScene(resolvedPath, scene));
        if (!m_LastOperationResult.Succeeded() || !scene) {
            return false;
        }

        if (!WarmupSceneAssets(scene)) {
            return false;
        }

        SetSceneDocument(scene, resolvedPath, SceneDocumentSource::LoadedFromDisk);
        m_ProjectSession.LastOpenedScenePath = resolvedPath;
        SyncWorkbenchSessionState();
        m_InteractionHost.MarkSaved();
        RefreshWorkbenchValidation();
        PersistCurrentProjectSession();
        return true;
    }

    bool EditorLayer::SaveActiveSceneDocument() {
        if (!m_SceneDocument.IsLoaded()) {
            auto result = ResultEnvelope::Failure("editor.workbench.save_scene", "editor.workbench", "There is no active scene document to save");
            result.AddDetail({ DiagnosticSeverity::Warning, "editor.workbench.save_scene.scene_missing", "Create or open a scene before saving", {} });
            CaptureOperationResult(result);
            return false;
        }

        if (!m_SceneDocument.ScenePath.empty()) {
            return SaveActiveSceneDocumentAs(m_SceneDocument.ScenePath);
        }

        const auto defaultPath = m_ProjectSession.Context.GetAssetRootPath() / MakeSceneFileName(m_SceneDocument.DisplayName);
        return SaveActiveSceneDocumentAs(defaultPath);
    }

    bool EditorLayer::SaveActiveSceneDocumentAs(const std::filesystem::path& scenePath) {
        if (!m_ProjectSession.IsLoaded() || !m_SceneDocument.SceneRef) {
            auto result = ResultEnvelope::Failure("editor.workbench.save_scene_as", "editor.workbench", "A loaded project and scene document are required before saving");
            result.AddDetail({ DiagnosticSeverity::Error, "editor.workbench.save_scene_as.scene_missing", "Create or open a scene before saving it", {} });
            CaptureOperationResult(result);
            return false;
        }

        auto resolvedPath = ResolveScenePathInput(scenePath);
        if (resolvedPath.extension() != ".scene") {
            resolvedPath += ".scene";
        }

        CaptureOperationResult(Application::GetInstance().GetOperations().SaveScene(*m_SceneDocument.SceneRef, resolvedPath));
        if (!m_LastOperationResult.Succeeded()) {
            return false;
        }

        m_SceneDocument.MarkSaved(resolvedPath);
        m_InteractionHost.MarkSaved();
        m_ProjectSession.LastOpenedScenePath = resolvedPath;
        SyncSceneDocumentState();
        SyncWorkbenchSessionState();
        RefreshCommandInputs();
        RefreshWorkbenchValidation();
        PersistCurrentProjectSession();
        return true;
    }

    void EditorLayer::RequestWorkbenchAction(const WorkbenchActionRequest& action) {
        if (action.Type == WorkbenchActionType::None) {
            return;
        }

        if (m_SceneDocument.IsLoaded() && m_SceneDocument.Dirty) {
            switch (action.Type) {
                case WorkbenchActionType::OpenProject:
                case WorkbenchActionType::CloseProject:
                case WorkbenchActionType::NewScene:
                case WorkbenchActionType::OpenScene:
                    m_PendingAction = action;
                    m_OpenUnsavedChangesPopup = true;
                    return;
                default:
                    break;
            }
        }

        ExecuteWorkbenchAction(action);
    }

    bool EditorLayer::ExecuteWorkbenchAction(const WorkbenchActionRequest& action) {
        switch (action.Type) {
            case WorkbenchActionType::OpenProject:
                return OpenProjectFromPath(action.Path, action.CreateIfMissing, action.Name, action.SeedInitialScene);
            case WorkbenchActionType::CloseProject:
                CloseProjectSession(true, "Project session was closed");
                return true;
            case WorkbenchActionType::NewScene:
                return CreateNewSceneDocument(action.Name.empty() ? m_Specification.InitialSceneName : action.Name);
            case WorkbenchActionType::OpenScene:
                return OpenSceneDocument(action.Path);
            default:
                return true;
        }
    }

    void EditorLayer::OnUpdate() {
        if (m_Mode != EditorWorkbenchMode::WorkbenchShell || !m_WorkbenchReady || !m_SceneDocument.SceneRef) {
            return;
        }

		m_EditorCameraController->Update(m_IsSceneViewportHovered);
		const auto camera = m_EditorCameraController->BuildRenderCamera();
		const auto renderResult = Application::GetInstance().GetOperations().RenderSceneViewport(*m_SceneDocument.SceneRef, camera);
        if (!renderResult.Succeeded()) {
            CaptureOperationResult(renderResult);
            m_WorkbenchReady = false;
        }
        m_SceneDocument.SceneRef->Update();
    }

	void EditorLayer::OnEvent(Event& event) {
		if (m_EditorCameraController && m_IsSceneViewportHovered) {
			m_EditorCameraController->OnEvent(event);
		}
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

    void EditorLayer::RecordWorkbenchInfoEvent(std::string_view operation, std::string_view target, std::string_view summary) {
        auto result = ResultEnvelope::Success(std::string(operation), std::string(target), std::string(summary));
        m_WorkbenchState.RecordEvent(result, "editor.workbench");
    }

    void EditorLayer::RefreshWorkbenchValidation() {
        if (!m_ProjectSession.IsLoaded() && !m_SceneDocument.SceneRef) {
            return;
        }

        auto& operations = Application::GetInstance().GetOperations();
        ValidationReport report;
        ApplicationValidationRequest request;
        request.Project = m_ProjectSession.IsLoaded() ? &m_ProjectSession.Context : nullptr;
        request.SceneTarget = m_SceneDocument.SceneRef.get();
        request.IncludeAssets = m_ProjectSession.IsLoaded();

        auto result = operations.Validate(request, &report);
        m_WorkbenchState.CaptureValidation(result, report, "editor.validation");
    }

    bool EditorLayer::ExecuteEditorCommand(EditorCommandPtr command) {
        if (!command) {
            auto result = ResultEnvelope::Failure("editor.command.dispatch", "editor.command", "No editor command was provided");
            CaptureOperationResult(result);
            return false;
        }

        CaptureOperationResult(m_InteractionHost.ExecuteCommand(std::move(command)));
        if (m_LastOperationResult.Succeeded()) {
            RefreshWorkbenchValidation();
        }
        return m_LastOperationResult.Succeeded();
    }

    void EditorLayer::ExecuteUndo() {
        CaptureOperationResult(m_InteractionHost.Undo());
        if (m_LastOperationResult.Succeeded()) {
            RefreshWorkbenchValidation();
        }
    }

    void EditorLayer::ExecuteRedo() {
        CaptureOperationResult(m_InteractionHost.Redo());
        if (m_LastOperationResult.Succeeded()) {
            RefreshWorkbenchValidation();
        }
    }

    std::string EditorLayer::MakeDefaultEntityName() const {
        if (!m_SceneDocument.SceneRef) {
            return "Entity";
        }

        size_t suffix = 1;
        while (true) {
            const std::string candidate = "Entity " + std::to_string(suffix);
            bool exists = false;
            m_SceneDocument.SceneRef->GetWorld().ForEachEntity([&](Entity entity) {
                if (entity.GetName() == candidate) {
                    exists = true;
                }
            });

            if (!exists) {
                return candidate;
            }

            ++suffix;
        }
    }

    void EditorLayer::CreateEntityFromHierarchy() {
        ExecuteEditorCommand(CreateCreateEntityCommand(MakeDefaultEntityName()));
    }

    void EditorLayer::DeleteSelectedEntities() {
        if (!Selection::HasSelection() || !m_SceneDocument.SceneRef) {
            return;
        }

        ExecuteEditorCommand(CreateDeleteEntitiesCommand(Selection::ResolveSelections(m_SceneDocument.SceneRef->GetWorld())));
    }

    void EditorLayer::AddComponentToPrimarySelection(EditorInspectableComponent type) {
        if (!Selection::HasSingleSelection() || !m_SceneDocument.SceneRef) {
            return;
        }

        ExecuteEditorCommand(CreateAddComponentCommand(type, Selection::ResolvePrimarySelection(m_SceneDocument.SceneRef->GetWorld())));
    }

    void EditorLayer::RemoveComponentFromPrimarySelection(EditorInspectableComponent type) {
        if (!Selection::HasSingleSelection() || !m_SceneDocument.SceneRef) {
            return;
        }

        ExecuteEditorCommand(CreateRemoveComponentCommand(type, Selection::ResolvePrimarySelection(m_SceneDocument.SceneRef->GetWorld())));
    }

    void EditorLayer::HandleGlobalShortcuts() {
        m_InteractionHost.Shortcuts().DispatchTriggered();
    }

    void EditorLayer::OnUnsavedChangesPopup() {
        if (m_OpenUnsavedChangesPopup) {
            ImGui::OpenPopup("Unsaved Scene Changes");
            m_OpenUnsavedChangesPopup = false;
        }

        if (ImGui::BeginPopupModal("Unsaved Scene Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped("The current scene has unsaved changes. Save before continuing?");
            ImGui::Spacing();

            if (ImGui::Button("Save and Continue")) {
                if (SaveActiveSceneDocument()) {
                    const auto action = m_PendingAction;
                    m_PendingAction = {};
                    ImGui::CloseCurrentPopup();
                    ExecuteWorkbenchAction(action);
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Discard and Continue")) {
                const auto action = m_PendingAction;
                m_PendingAction = {};
                ImGui::CloseCurrentPopup();
                ExecuteWorkbenchAction(action);
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                m_PendingAction = {};
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    void EditorLayer::OnGuiRender() {
		ImGuizmo::BeginFrame();
        if (m_Mode == EditorWorkbenchMode::ProjectHub) {
            OnProjectHubShell();
            OnUnsavedChangesPopup();
            return;
        }

        OnDockingPanel();
        HandleGlobalShortcuts();

        if (!m_WorkbenchReady) {
            ImGui::Begin("Workbench Status");
            ImGui::TextWrapped("%s", m_LastOperationResult.Summary.empty() ? "Editor workbench is not ready." : m_LastOperationResult.Summary.c_str());
            ImGui::Text("Operation: %s", m_LastOperationResult.Operation.c_str());
            ImGui::Spacing();
            if (ImGui::Button("Return to Project Hub")) {
                CloseProjectSession(true, "Returned to Project Hub from an unready workbench");
            }
            ImGui::SameLine();
            if (ImGui::Button("Retry Bind") && m_SceneDocument.SceneRef) {
                m_WorkbenchReady = BindSceneDocumentToShell();
            }
            ImGui::End();
            if (m_ShowConsolePanel) {
                m_Concole->OnGuiRender();
            }
            OnUnsavedChangesPopup();
            return;
        }

        if (m_ShowProjectPanel && m_ProjectPanel) {
            m_ProjectPanel->OnGuiRender();
            if (const auto action = m_ProjectPanel->ConsumePendingAction()) {
                switch (action->Type) {
                    case ProjectPanelActionType::OpenScene:
                        RequestWorkbenchAction({ WorkbenchActionType::OpenScene, action->Path });
                        break;
                    case ProjectPanelActionType::RefreshProject:
                        if (m_ProjectSession.IsLoaded()) {
                            ProjectStatusReport status;
                            CaptureOperationResult(Application::GetInstance().GetOperations().CheckProjectStatus(m_ProjectSession.Context, &status));
                            if (m_LastOperationResult.Succeeded()) {
                                m_ProjectSession.LastStatus = status;
                                SyncWorkbenchSessionState();
                                RefreshWorkbenchValidation();
                                RecordWorkbenchInfoEvent(
                                    "editor.workbench.refresh_project",
                                    m_ProjectSession.Context.GetTargetId(),
                                    "Project status refreshed");
                            }
                        }
                        break;
                    default:
                        break;
                }
            }
        }

        if (m_ShowScenePanel) {
            OnScenePanel();
        }
        if (m_ShowHierarchyPanel && m_HierarchyPanel) {
            m_HierarchyPanel->OnGuiRender();
        }
        const bool inspectorChanged = m_ShowInspectorPanel ? m_Inspector->OnGuiRender() : false;
        if (inspectorChanged && m_SceneDocument.IsLoaded()) {
            CaptureOperationResult(m_InteractionHost.MarkExternalSceneMutation(
                "editor.inspector.modify_components",
                m_SceneDocument.ScenePath.empty() ? "scene:new" : m_SceneDocument.ScenePath.generic_string(),
                "Inspector changes updated the active scene document"));
            RefreshWorkbenchValidation();
        }
        if (m_ShowConsolePanel) {
            m_Concole->OnGuiRender();
        }
        OnUnsavedChangesPopup();
    }

    void EditorLayer::OnProjectHubShell() {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(32.0f, 32.0f));

        constexpr ImGuiWindowFlags windowFlags =
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus;

        ImGui::Begin("Project Hub Shell", nullptr, windowFlags);
        ImGui::PopStyleVar(3);

        const ImVec2 availableRegion = ImGui::GetContentRegionAvail();
        const float cardWidth = (std::min)(availableRegion.x, 760.0f);
        const float cardHeight = (std::min)(availableRegion.y, 520.0f);
        const ImVec2 cursor = ImGui::GetCursorPos();
        const float offsetX = (std::max)(0.0f, (availableRegion.x - cardWidth) * 0.5f);
        const float offsetY = (std::max)(0.0f, (availableRegion.y - cardHeight) * 0.5f);
        ImGui::SetCursorPos(ImVec2(cursor.x + offsetX, cursor.y + offsetY));

        ImGui::BeginChild("Project Hub Card", ImVec2(cardWidth, cardHeight), true, ImGuiWindowFlags_None);
        OnProjectHubPanel();
        ImGui::EndChild();

        ImGui::End();
    }

    void EditorLayer::OnProjectHubPanel() {
        ImGui::TextUnformatted("Editor requires an active project.");
        ImGui::Separator();

        if (m_HasPersistedSession) {
            ImGui::Text("Last Project: %s", m_PersistedSession.LastProjectName.empty() ? "<unnamed>" : m_PersistedSession.LastProjectName.c_str());
            ImGui::TextWrapped("Root: %s", m_PersistedSession.LastProjectRoot.c_str());
            if (!m_PersistedSession.LastScenePath.empty()) {
                ImGui::TextWrapped("Last Scene: %s", m_PersistedSession.LastScenePath.c_str());
            }
            if (ImGui::Button("Open Last Project")) {
                ResumePersistedProjectSession();
            }
            ImGui::SameLine();
            if (ImGui::Button("Launch Project Hub")) {
                if (HostLaunch::LaunchSibling("ProjectHub.exe")) {
                    Application::GetInstance().RequestShutdown();
                } else {
                    auto result = ResultEnvelope::Failure("editor.launch_project_hub", "ProjectHub.exe", "Failed to launch Project Hub");
                    result.AddDetail({ DiagnosticSeverity::Error, "editor.launch_project_hub.failed", "ProjectHub executable could not be launched from the current output directory", HostLaunch::ResolveSiblingExecutable("ProjectHub.exe").generic_string() });
                    CaptureOperationResult(result);
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Reset Saved Session")) {
                ClearPersistedProjectSession();
            }
            ImGui::Separator();
        } else {
            ImGui::TextUnformatted("No saved project session is available.");
            if (ImGui::Button("Launch Project Hub")) {
                if (HostLaunch::LaunchSibling("ProjectHub.exe")) {
                    Application::GetInstance().RequestShutdown();
                } else {
                    auto result = ResultEnvelope::Failure("editor.launch_project_hub", "ProjectHub.exe", "Failed to launch Project Hub");
                    result.AddDetail({ DiagnosticSeverity::Error, "editor.launch_project_hub.failed", "ProjectHub executable could not be launched from the current output directory", HostLaunch::ResolveSiblingExecutable("ProjectHub.exe").generic_string() });
                    CaptureOperationResult(result);
                }
            }
        }

        ImGui::Spacing();
        if (ImGui::Button("Quit Editor")) {
            Application::GetInstance().RequestShutdown();
        }

        ImGui::Spacing();
        ImGui::TextWrapped("Project creation and open flows now live in the standalone ProjectHub host. Editor remains the project-bound workbench and can still be launched directly with `--project <path>`.");
    }

    void EditorLayer::OnDockingPanel() {
        static bool enable_docking = true;

        static bool opt_fullscreen = true;
        static bool opt_padding = false;
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

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

        if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
            window_flags |= ImGuiWindowFlags_NoBackground;

        if (!opt_padding)
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("Hua Engine", &enable_docking, window_flags);
        if (!opt_padding)
            ImGui::PopStyleVar();

        if (opt_fullscreen)
            ImGui::PopStyleVar(2);

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
            if (ImGui::BeginMenu("Project")) {
                if (ImGui::MenuItem("Resume Last Project", nullptr, false, m_HasPersistedSession && !m_ProjectSession.IsLoaded())) {
                    ResumePersistedProjectSession();
                }
                if (ImGui::MenuItem("Close Project", nullptr, false, m_ProjectSession.IsLoaded())) {
                    RequestWorkbenchAction({ WorkbenchActionType::CloseProject });
                }
                if (ImGui::MenuItem("Reset Saved Session", nullptr, false, m_HasPersistedSession)) {
                    ClearPersistedProjectSession();
                }
                if (ImGui::MenuItem("Refresh Project Status", nullptr, false, m_ProjectSession.IsLoaded())) {
                    ProjectStatusReport status;
                    CaptureOperationResult(Application::GetInstance().GetOperations().CheckProjectStatus(m_ProjectSession.Context, &status));
                    if (m_LastOperationResult.Succeeded()) {
                        m_ProjectSession.LastStatus = status;
                        SyncWorkbenchSessionState();
                        RefreshWorkbenchValidation();
                    }
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Scene")) {
                if (ImGui::MenuItem("New Scene...", nullptr, false, m_ProjectSession.IsLoaded())) {
                    CopyToBuffer(m_Specification.InitialSceneName, m_NewSceneNameInput.data(), m_NewSceneNameInput.size());
                    ImGui::OpenPopup("New Scene");
                }
                if (ImGui::MenuItem("Open Scene...", nullptr, false, m_ProjectSession.IsLoaded())) {
                    ImGui::OpenPopup("Open Scene");
                }
                if (ImGui::MenuItem("Save Scene", "Ctrl+S", false, m_SceneDocument.IsLoaded())) {
                    SaveActiveSceneDocument();
                }
                if (ImGui::MenuItem("Save Scene As...", nullptr, false, m_SceneDocument.IsLoaded())) {
                    ImGui::OpenPopup("Save Scene As");
                }
                if (ImGui::MenuItem("Validate Scene", nullptr, false, m_SceneDocument.IsLoaded())) {
                    CaptureOperationResult(Application::GetInstance().GetOperations().ValidateScene(*m_SceneDocument.SceneRef));
                    RefreshWorkbenchValidation();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Edit")) {
                const auto undoLabel = m_InteractionHost.GetUndoLabel();
                const auto redoLabel = m_InteractionHost.GetRedoLabel();
                std::string undoTitle = undoLabel.empty() ? "Undo" : ("Undo " + undoLabel);
                std::string redoTitle = redoLabel.empty() ? "Redo" : ("Redo " + redoLabel);

                if (ImGui::MenuItem(undoTitle.c_str(), "Ctrl+Z", false, m_InteractionHost.CanUndo())) {
                    ExecuteUndo();
                }

                if (ImGui::MenuItem(redoTitle.c_str(), "Ctrl+Y", false, m_InteractionHost.CanRedo())) {
                    ExecuteRedo();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Project", nullptr, &m_ShowProjectPanel);
                ImGui::MenuItem("Hierarchy", nullptr, &m_ShowHierarchyPanel);
                ImGui::MenuItem("Inspector", nullptr, &m_ShowInspectorPanel);
                ImGui::MenuItem("Console", nullptr, &m_ShowConsolePanel);
                ImGui::MenuItem("Scene", nullptr, &m_ShowScenePanel);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Options"))
            {
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

        if (ImGui::BeginPopupModal("New Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::InputText("Scene Name", m_NewSceneNameInput.data(), static_cast<int>(m_NewSceneNameInput.size()));
            if (ImGui::Button("Create")) {
                RequestWorkbenchAction({ WorkbenchActionType::NewScene, {}, m_NewSceneNameInput.data() });
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupModal("Open Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::InputText("Scene Path", m_SceneOpenPathInput.data(), static_cast<int>(m_SceneOpenPathInput.size()));
            if (ImGui::Button("Open")) {
                RequestWorkbenchAction({ WorkbenchActionType::OpenScene, m_SceneOpenPathInput.data() });
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupModal("Save Scene As", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::InputText("Scene Path", m_SceneSaveAsPathInput.data(), static_cast<int>(m_SceneSaveAsPathInput.size()));
            if (ImGui::Button("Save")) {
                if (SaveActiveSceneDocumentAs(m_SceneSaveAsPathInput.data())) {
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
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
        ImGuiID leftColumnDockId = ImGui::DockBuilderSplitNode(centerDockId, ImGuiDir_Left, 0.30f, nullptr, &centerDockId);
        ImGuiID rightDockId = ImGui::DockBuilderSplitNode(centerDockId, ImGuiDir_Right, 0.28f, nullptr, &centerDockId);
        ImGuiID bottomDockId = ImGui::DockBuilderSplitNode(centerDockId, ImGuiDir_Down, 0.28f, nullptr, &centerDockId);
        ImGuiID projectDockId = leftColumnDockId;
        ImGuiID hierarchyDockId = ImGui::DockBuilderSplitNode(projectDockId, ImGuiDir_Down, 0.45f, nullptr, &projectDockId);

        ImGui::DockBuilderDockWindow("Project", projectDockId);
        ImGui::DockBuilderDockWindow("Hierarchy", hierarchyDockId);
        ImGui::DockBuilderDockWindow("Inspector", rightDockId);
        ImGui::DockBuilderDockWindow("Console", bottomDockId);
        ImGui::DockBuilderDockWindow("Scene", centerDockId);
        ImGui::DockBuilderFinish(dockspaceId);
    }

    void EditorLayer::OnScenePanel() {
        if (!m_RenderTarget) {
            return;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
        ImGui::Begin("Scene");
        if (!m_SceneDocument.SceneRef) {
            ImGui::TextUnformatted("No scene loaded.");
            ImGui::TextWrapped("Use the Scene menu or the Project panel to create or open a scene document inside the current project.");
            ImGui::End();
            ImGui::PopStyleVar();
            return;
        }

        ImGui::TextDisabled("%s%s", m_SceneDocument.DisplayName.c_str(), m_SceneDocument.Dirty ? " *" : "");
        ImGui::Separator();

        ImVec2 scenePanelSize = ImGui::GetContentRegionAvail();
        if (scenePanelSize.x < 1.0f) scenePanelSize.x = 1.0f;
        if (scenePanelSize.y < 1.0f) scenePanelSize.y = 1.0f;

        const glm::vec2 newViewportSize = { scenePanelSize.x, scenePanelSize.y };
        if (newViewportSize != m_SceneViewportSize) {
            m_RenderTarget->Resize((uint32_t)scenePanelSize.x, (uint32_t)scenePanelSize.y);
            m_SceneViewportSize = newViewportSize;
        }

        const ImVec2 viewportOrigin = ImGui::GetCursorScreenPos();
        ImGui::Image(m_RenderTarget->GetColorAttachmentView().NativeHandle,
            { m_SceneViewportSize.x , m_SceneViewportSize.y },
            { 0, 1 }, { 1, 0 });
		m_IsSceneViewportHovered = ImGui::IsItemHovered();
        m_EditorCameraController->SetViewport(m_SceneViewportSize.x, m_SceneViewportSize.y);

        if (ImGui::IsWindowHovered()) {
            if (ImGui::IsKeyPressed(ImGuiKey_W)) m_GizmoOperation = ImGuizmo::TRANSLATE;
            if (ImGui::IsKeyPressed(ImGuiKey_E)) m_GizmoOperation = ImGuizmo::ROTATE;
            if (ImGui::IsKeyPressed(ImGuiKey_R)) m_GizmoOperation = ImGuizmo::SCALE;
        }

        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(viewportOrigin.x, viewportOrigin.y, m_SceneViewportSize.x, m_SceneViewportSize.y);
        ImGuizmo::SetOrthographic(false);
		ImGuizmo::AllowAxisFlip(false);
		ImGuizmo::SetAxisLimit(0.0f);
		const auto camera = m_EditorCameraController->BuildRenderCamera();
        auto selectedEntity = Selection::HasSingleSelection()
            ? Selection::ResolvePrimarySelection(m_SceneDocument.SceneRef->GetWorld())
            : Entity{};
        if (selectedEntity.IsValid() && selectedEntity.HasComponent<TransformComponent>()) {
            auto& transform = selectedEntity.GetComponent<TransformComponent>();
            auto transformMatrix = transform.GetTransformMat();
            ImGuizmo::Manipulate(
                glm::value_ptr(camera.GetView()),
                glm::value_ptr(camera.GetProjection()),
                static_cast<ImGuizmo::OPERATION>(m_GizmoOperation),
                ImGuizmo::LOCAL,
                glm::value_ptr(transformMatrix));

            const bool isUsing = ImGuizmo::IsUsing();
            if (isUsing) {
                if (!m_GizmoWasUsing || m_GizmoEntityUuid != selectedEntity.GetUuid()) {
                    m_GizmoInitialTransform = transform;
                    m_GizmoEntityUuid = selectedEntity.GetUuid();
                }

                float translation[3];
                float rotationDegrees[3];
                float scale[3];
                ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(transformMatrix), translation, rotationDegrees, scale);
                transform.Position = { translation[0], translation[1], translation[2] };
                transform.Rotation = glm::radians(glm::vec3(rotationDegrees[0], rotationDegrees[1], rotationDegrees[2]));
                transform.Scale = { scale[0], scale[1], scale[2] };
            } else if (m_GizmoWasUsing && m_GizmoEntityUuid == selectedEntity.GetUuid()) {
                ExecuteEditorCommand(CreateSetTransformCommand(selectedEntity, m_GizmoInitialTransform, transform));
                m_GizmoEntityUuid = {};
            }
            m_GizmoWasUsing = isUsing;
        } else {
            m_GizmoWasUsing = false;
            m_GizmoEntityUuid = {};
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }
}
