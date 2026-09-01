#include "enginepch.h"
#include "EditorLayer.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <system_error>
#include <vector>

#include "HuaEngine/Application/ApplicationOperations.h"
#include "HuaEngine/Core/HostLaunch.h"
#include "HuaEngine/Asset/Import/ShaderDescriptor.h"
#include "HuaEngine/Rendering/RHI/RenderHardwareInterface.h"
#include "Input/EditorInputBindingStorage.h"
#include "Interaction/EditorSceneCommands.h"
#include "ImGuizmo.h"
#include "imgui.h"
#include <imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>
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

        uint32_t DecodeObjectId(Rendering::RenderTargetPixelRGBA8 pixel) {
            return static_cast<uint32_t>(pixel.R)
                | (static_cast<uint32_t>(pixel.G) << 8u)
                | (static_cast<uint32_t>(pixel.B) << 16u)
                | (static_cast<uint32_t>(pixel.A) << 24u);
        }

    }

    EditorLayer::EditorLayer(EditorLayerSpecification specification)
        : Layer("EditorLayer"), m_Specification(std::move(specification)) {
        m_EditorCameraController = CreateRef<Editor::EditorCameraController>();
        m_ProjectPanel.reset(new ProjectPanel());
        m_ProjectPanel->SetWorkbenchState(&m_WorkbenchState);
		m_ProjectPanel->SetCanReimportCallback([](const std::filesystem::path& sourcePath) {
			return Application::GetInstance().GetOperations().CanImportAssetSource(sourcePath);
		});
		m_AssetInspectorEditor = CreateRef<Editor::AssetInspectorEditor>(m_AssetPickerCatalog);
		m_AssetInspectorEditor->SetWorkbenchState(&m_WorkbenchState);
		m_AssetInspectorEditor->SetOpenSceneCallback([this](const std::filesystem::path& path) {
			RequestWorkbenchAction({ WorkbenchActionType::OpenScene, path });
		});
		m_SceneEntityInspectorEditor = CreateRef<Editor::SceneEntityInspectorEditor>(m_AssetPickerCatalog);
		m_SceneEntityInspectorEditor->SetWorkbenchState(&m_WorkbenchState);
        m_Inspector.reset(new InspectorPanel(*m_AssetInspectorEditor, *m_SceneEntityInspectorEditor));
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
		m_AssetInspectorEditor->BindProject(&m_ProjectSession.Context);
        m_ProjectSession.LastStatus = status;
        m_ProjectSession.Loaded = true;

        if (m_HasPersistedSession && !m_PersistedSession.LastProjectRoot.empty()) {
			const auto persistedRoot = NormalizePath(m_PersistedSession.LastProjectRoot);
            if (persistedRoot == context.RootPath && !m_PersistedSession.LastScenePath.empty()) {
                m_ProjectSession.LastOpenedScenePath = NormalizePath(m_PersistedSession.LastScenePath);
            }
        }

		m_EditorCameraController->ResetPose();

        SyncWorkbenchSessionState();
        RefreshInteractionHost();
        RecordWorkbenchInfoEvent(
            "editor.workbench.project_session_ready",
            m_ProjectSession.Context.GetTargetId(),
            "Project session is ready");

		CaptureOperationResult(Application::GetInstance().GetOperations().InitializeProjectAssets(context));
		if (m_LastOperationResult.Failed()) {
			CloseProjectSession(true, "Project assets failed to initialize");
			return false;
		}
		if (!RefreshAssetPickerCatalog()) {
			CloseProjectSession(true, "Project asset catalog failed to load");
			return false;
		}

        if (!InitializeWorkbenchShell()) {
            CloseProjectSession(true, "Workbench shell failed to initialize");
            return false;
        }

        bool restoredScene = false;
        if (!seedInitialScene) {
            restoredScene = RestoreLastSceneForSession();
        }

        if (!restoredScene && seedInitialScene) {
            if (!CreateNewSceneDocument(m_Specification.InitialSceneName)) {
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

	void EditorLayer::RestoreSceneCameraPose(const std::filesystem::path& scenePath) {
		m_EditorCameraController->ResetPose();
		const auto* pose = m_PersistedSession.FindSceneCameraPose(NormalizePath(scenePath).generic_string());
		if (pose) {
			m_EditorCameraController->SetPose({ pose->PositionX, pose->PositionY, pose->PositionZ }, pose->Pitch, pose->Yaw);
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
		m_AssetInspectorEditor->BindProject(nullptr);
        if (preserveResumeState && m_ProjectSession.IsLoaded()) {
            PersistCurrentProjectSession();
        } else if (!preserveResumeState) {
            ClearPersistedProjectSession();
        }

        m_ProjectSession.Reset();
        m_SceneDocument.Reset();
		m_AssetPickerCatalog.Clear();
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
        m_InteractionHost.Input().Reset();
        m_InteractionHost.DragDrop().Clear();

		auto& input = m_InteractionHost.Input();
		auto registerCommand = [&](Editor::EditorCommandDescriptor descriptor) {
			(void)input.Commands().Register(std::move(descriptor));
		};
		registerCommand({ "editor.undo", "Undo", "Edit", [this]() { return m_InteractionHost.CanUndo(); }, [this]() { ExecuteUndo(); } });
		registerCommand({ "editor.redo", "Redo", "Edit", [this]() { return m_InteractionHost.CanRedo(); }, [this]() { ExecuteRedo(); } });
		registerCommand({ "editor.entity.create", "New Entity", "Scene", [this]() { return m_InteractionHost.HasActiveScene(); }, [this]() { CreateEntityFromHierarchy(); } });
		registerCommand({ "editor.entity.delete", "Delete Selected", "Scene", []() { return Selection::HasSelection(); }, [this]() { DeleteSelectedEntities(); } });
		registerCommand({
			"editor.scene.save", "Save", "Scene",
			[this]() { return m_SceneDocument.IsLoaded() || (Selection::HasAssetSelection() && m_AssetInspectorEditor->HasDirtyEdit()); },
			[this]() {
				if (Selection::HasAssetSelection() && m_AssetInspectorEditor->HasDirtyEdit()) {
					(void)m_AssetInspectorEditor->Apply();
					return;
				}
				SaveActiveSceneDocument();
			}
		});
		registerCommand({ "editor.project.resume", "Resume Last Project", "Project", [this]() { return m_HasPersistedSession && !m_ProjectSession.IsLoaded(); }, [this]() { ResumePersistedProjectSession(); } });
		registerCommand({ "editor.project.close", "Close Project", "Project", [this]() { return m_ProjectSession.IsLoaded(); }, [this]() { RequestWorkbenchAction({ WorkbenchActionType::CloseProject }); } });
		registerCommand({ "editor.project.reset_session", "Reset Saved Session", "Project", [this]() { return m_HasPersistedSession; }, [this]() { ClearPersistedProjectSession(); } });
		registerCommand({
			"editor.project.refresh", "Refresh Project Status", "Project", [this]() { return m_ProjectSession.IsLoaded(); },
			[this]() {
				ProjectStatusReport status;
				CaptureOperationResult(Application::GetInstance().GetOperations().CheckProjectStatus(m_ProjectSession.Context, &status));
				if (m_LastOperationResult.Succeeded()) {
					m_ProjectSession.LastStatus = status;
					SyncWorkbenchSessionState();
					RefreshWorkbenchValidation();
				}
			}
		});
		registerCommand({ "editor.scene.new", "New Scene...", "Scene", [this]() { return m_ProjectSession.IsLoaded(); }, [this]() { CopyToBuffer(m_Specification.InitialSceneName, m_NewSceneNameInput.data(), m_NewSceneNameInput.size()); m_RequestNewScenePopup = true; } });
		registerCommand({ "editor.scene.open", "Open Scene...", "Scene", [this]() { return m_ProjectSession.IsLoaded(); }, [this]() { m_RequestOpenScenePopup = true; } });
		registerCommand({ "editor.scene.save_as", "Save Scene As...", "Scene", [this]() { return m_SceneDocument.IsLoaded(); }, [this]() { m_RequestSaveSceneAsPopup = true; } });
		registerCommand({
			"editor.scene.validate", "Validate Scene", "Scene", [this]() { return m_SceneDocument.IsLoaded(); },
			[this]() {
				CaptureOperationResult(Application::GetInstance().GetOperations().ValidateScene(*m_SceneDocument.SceneRef));
				RefreshWorkbenchValidation();
			}
		});
		registerCommand({ "editor.gizmo.translate", "Translate Tool", "Scene", []() { return true; }, [this]() { m_GizmoOperation = ImGuizmo::TRANSLATE; } });
		registerCommand({ "editor.gizmo.rotate", "Rotate Tool", "Scene", []() { return true; }, [this]() { m_GizmoOperation = ImGuizmo::ROTATE; } });
		registerCommand({ "editor.gizmo.scale", "Scale Tool", "Scene", []() { return true; }, [this]() { m_GizmoOperation = ImGuizmo::SCALE; } });

		const auto ctrl = InputModifiers::Control;
		const auto ctrlShift = InputModifiers::Control | InputModifiers::Shift;
		(void)input.Bindings().RegisterDefaultCommand({ "undo", "editor.undo", "Global", { KeyboardControl(Key::Z), ctrl, InputTrigger::Pressed, true }, 0, true });
		(void)input.Bindings().RegisterDefaultCommand({ "redo", "editor.redo", "Global", { KeyboardControl(Key::Y), ctrl, InputTrigger::Pressed, true }, 0, true });
		(void)input.Bindings().RegisterDefaultCommand({ "entity.create", "editor.entity.create", "Global", { KeyboardControl(Key::N), ctrlShift, InputTrigger::Pressed, true }, 0, true });
		(void)input.Bindings().RegisterDefaultCommand({ "entity.delete", "editor.entity.delete", "Global", { KeyboardControl(Key::Delete), InputModifiers::None, InputTrigger::Pressed, true }, 0, true });
		(void)input.Bindings().RegisterDefaultCommand({ "scene.save", "editor.scene.save", "Global", { KeyboardControl(Key::S), ctrl, InputTrigger::Pressed, true }, 0, true });
		(void)input.Bindings().RegisterDefaultCommand({ "gizmo.translate", "editor.gizmo.translate", "SceneViewport", { KeyboardControl(Key::W), InputModifiers::None, InputTrigger::Pressed, true }, 0, true });
		(void)input.Bindings().RegisterDefaultCommand({ "gizmo.rotate", "editor.gizmo.rotate", "SceneViewport", { KeyboardControl(Key::E), InputModifiers::None, InputTrigger::Pressed, true }, 0, true });
		(void)input.Bindings().RegisterDefaultCommand({ "gizmo.scale", "editor.gizmo.scale", "SceneViewport", { KeyboardControl(Key::R), InputModifiers::None, InputTrigger::Pressed, true }, 0, true });
		auto registerAction = [&](Editor::EditorActionBinding binding) { (void)input.Bindings().RegisterDefaultAction(std::move(binding)); };
		registerAction({ .Id = "camera.forward", .ActionId = "editor.camera.forward", .ContextId = "SceneViewport", .Gesture = { KeyboardControl(Key::W), InputModifiers::None, InputTrigger::Held, true }, .Scale = 1.0f });
		registerAction({ .Id = "camera.backward", .ActionId = "editor.camera.forward", .ContextId = "SceneViewport", .Gesture = { KeyboardControl(Key::S), InputModifiers::None, InputTrigger::Held, true }, .Scale = -1.0f });
		registerAction({ .Id = "camera.right", .ActionId = "editor.camera.right", .ContextId = "SceneViewport", .Gesture = { KeyboardControl(Key::D), InputModifiers::None, InputTrigger::Held, true }, .Scale = 1.0f });
		registerAction({ .Id = "camera.left", .ActionId = "editor.camera.right", .ContextId = "SceneViewport", .Gesture = { KeyboardControl(Key::A), InputModifiers::None, InputTrigger::Held, true }, .Scale = -1.0f });
		registerAction({ .Id = "camera.look_x", .ActionId = "editor.camera.look_x", .ContextId = "SceneViewport", .Gesture = { MouseControl(Mouse::ButtonRight), InputModifiers::None, InputTrigger::Held, true }, .Scale = 1.0f, .ValueSource = Editor::EditorActionValueSource::PointerDeltaX });
		registerAction({ .Id = "camera.look_y", .ActionId = "editor.camera.look_y", .ContextId = "SceneViewport", .Gesture = { MouseControl(Mouse::ButtonRight), InputModifiers::None, InputTrigger::Held, true }, .Scale = 1.0f, .ValueSource = Editor::EditorActionValueSource::PointerDeltaY });
		registerAction({ .Id = "camera.pan_x", .ActionId = "editor.camera.pan_x", .ContextId = "SceneViewport", .Gesture = { MouseControl(Mouse::ButtonMiddle), InputModifiers::None, InputTrigger::Held, true }, .Scale = 1.0f, .ValueSource = Editor::EditorActionValueSource::PointerDeltaX });
		registerAction({ .Id = "camera.pan_y", .ActionId = "editor.camera.pan_y", .ContextId = "SceneViewport", .Gesture = { MouseControl(Mouse::ButtonMiddle), InputModifiers::None, InputTrigger::Held, true }, .Scale = 1.0f, .ValueSource = Editor::EditorActionValueSource::PointerDeltaY });
		registerAction({ .Id = "camera.zoom", .ActionId = "editor.camera.zoom", .ContextId = "SceneViewport", .Gesture = { MouseControl(Mouse::ButtonLast), InputModifiers::None, InputTrigger::Scrolled, true }, .Scale = 1.0f, .ValueSource = Editor::EditorActionValueSource::ScrollY });
		registerAction({ .Id = "scene.pick", .ActionId = "editor.scene.pick", .ContextId = "SceneViewport", .Gesture = { MouseControl(Mouse::ButtonLeft), InputModifiers::None, InputTrigger::Pressed, true } });
		registerAction({ .Id = "project.open", .ActionId = "editor.project.open_item", .ContextId = "Project", .Gesture = { MouseControl(Mouse::ButtonLeft), InputModifiers::None, InputTrigger::DoublePressed, true } });
		registerAction({ .Id = "hierarchy.select", .ActionId = "editor.hierarchy.select", .ContextId = "Hierarchy", .Gesture = { MouseControl(Mouse::ButtonLeft), InputModifiers::None, InputTrigger::Pressed, true } });
		registerAction({ .Id = "hierarchy.toggle", .ActionId = "editor.hierarchy.toggle", .ContextId = "Hierarchy", .Gesture = { MouseControl(Mouse::ButtonLeft), InputModifiers::Control, InputTrigger::Pressed, true } });
		registerAction({ .Id = "hierarchy.context", .ActionId = "editor.hierarchy.context_select", .ContextId = "Hierarchy", .Gesture = { MouseControl(Mouse::ButtonRight), InputModifiers::None, InputTrigger::Pressed, true } });
		std::vector<Editor::EditorInputBindingOverride> userOverrides;
		if (Editor::EditorInputBindingStorage::Load(Editor::EditorInputBindingStorage::GetDefaultPath(), userOverrides).Succeeded()) {
			(void)input.Bindings().SetOverrides(std::move(userOverrides));
		}

        m_InteractionHost.ContextMenus().Replace("hierarchy.window", {
            {
                .CommandId = "editor.entity.create",
                .Label = "New Entity",
                .Tooltip = "Create a new entity in the active scene",
				.Enabled = true
            }
        });
        m_InteractionHost.ContextMenus().Replace("hierarchy.entity", {
            {
                .CommandId = "editor.entity.create",
                .Label = "New Entity",
                .Tooltip = "Create a new entity in the active scene",
				.Enabled = true
            },
            {
                .CommandId = "editor.entity.delete",
                .Label = "Delete Selected",
                .Tooltip = "Delete the current selection and support undo/redo",
				.Enabled = true
            }
        });

        std::vector<ContextMenuActionDescriptor> inspectorActions;
        for (const auto& descriptor : GetEditorInspectableComponents()) {
			const auto commandId = "editor.component.remove." + descriptor.Id;
			registerCommand({
				commandId,
				"Remove " + descriptor.DisplayName + " Component",
				"Inspector",
				[this, type = descriptor.Type]() {
					if (!m_SceneDocument.SceneRef || !Selection::HasSingleSelection()) return false;
					return CanRemoveInspectableComponent(type, Selection::ResolvePrimarySelection(m_SceneDocument.SceneRef->GetWorld()));
				},
				[this, type = descriptor.Type]() { RemoveComponentFromPrimarySelection(type); }
			});
            inspectorActions.push_back({
                .CommandId = commandId,
                .Label = "Remove " + descriptor.DisplayName + " Component",
                .Tooltip = "Remove a component from the selected entity",
				.Enabled = true
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

		m_SceneEntityInspectorEditor->BindInteractionHost(&m_InteractionHost);
		m_SceneEntityInspectorEditor->SetAddComponentCallback([this](EditorInspectableComponent type) {
            AddComponentToPrimarySelection(type);
        });
		m_SceneEntityInspectorEditor->SetRemoveComponentCallback([this](EditorInspectableComponent type) {
            RemoveComponentFromPrimarySelection(type);
        });
        if (m_HierarchyPanel) {
            m_HierarchyPanel->SetInteractionHost(&m_InteractionHost);
        }
		m_ProjectPanel->SetInputService(&input);
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

	bool EditorLayer::RefreshAssetPickerCatalog() {
		if (!m_ProjectSession.IsLoaded()) {
			m_AssetPickerCatalog.Clear();
			return true;
		}

		std::vector<AssetRecord> records;
		auto result = Application::GetInstance().GetOperations().ListAssets(m_ProjectSession.Context, records);
		if (!result.Succeeded()) {
			CaptureOperationResult(result);
			m_AssetPickerCatalog.Clear();
			return false;
		}

		m_AssetPickerCatalog.Rebuild(records);
		m_ProjectPanel->SetAssetRecords(records);
		return true;
	}

	void EditorLayer::ReimportProjectAssets(const std::filesystem::path& targetPath) {
		if (m_AssetInspectorEditor && m_AssetInspectorEditor->RequestDirtyResolution([this, targetPath]() { ReimportProjectAssets(targetPath); })) return;
		if (!m_ProjectSession.IsLoaded()) {
			return;
		}

		auto result = Application::GetInstance().GetOperations().ReimportAssets(
			m_ProjectSession.Context,
			targetPath);
		CaptureOperationResult(result);
		RefreshAssetPickerCatalog();
		RefreshWorkbenchValidation();
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
		if (m_EditorCameraController && !m_SceneDocument.ScenePath.empty()) {
			const auto& position = m_EditorCameraController->GetPosition();
			m_PersistedSession.UpsertSceneCameraPose({
				.ScenePath = NormalizePath(m_SceneDocument.ScenePath).generic_string(),
				.PositionX = position.x,
				.PositionY = position.y,
				.PositionZ = position.z,
				.Pitch = m_EditorCameraController->GetPitch(),
				.Yaw = m_EditorCameraController->GetYaw()
			});
		}
		m_SceneCameraPoseDirty = false;
		m_LastSceneCameraPoseSave = std::chrono::steady_clock::now();
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
		m_ObjectIdRenderTarget = Rendering::RenderHardwareInterface::GetDevice().CreateRenderTarget({ .Specification = spec });
		m_SceneRenderExtension.SetObjectIdTarget(m_ObjectIdRenderTarget);

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

		m_EditorCameraController->ResetPose();
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

		PersistCurrentProjectSession();
		RestoreSceneCameraPose(resolvedPath);
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
		CaptureOperationResult(Application::GetInstance().GetOperations().RegisterSceneAsset(m_ProjectSession.Context, resolvedPath));
		if (!m_LastOperationResult.Succeeded()) return false;

        m_SceneDocument.MarkSaved(resolvedPath);
        m_InteractionHost.MarkSaved();
        m_ProjectSession.LastOpenedScenePath = resolvedPath;
        SyncSceneDocumentState();
        SyncWorkbenchSessionState();
        RefreshCommandInputs();
		RefreshAssetPickerCatalog();
        RefreshWorkbenchValidation();
        PersistCurrentProjectSession();
        return true;
    }

    void EditorLayer::RequestWorkbenchAction(const WorkbenchActionRequest& action) {
        if (action.Type == WorkbenchActionType::None) {
            return;
        }

		const bool hasUnsavedChanges = (m_SceneDocument.IsLoaded() && m_SceneDocument.Dirty) || (m_AssetInspectorEditor && m_AssetInspectorEditor->HasDirtyEdit());
		const bool actionLeavesDocument = action.Type == WorkbenchActionType::OpenProject || action.Type == WorkbenchActionType::CloseProject ||
			action.Type == WorkbenchActionType::NewScene || action.Type == WorkbenchActionType::OpenScene || action.Type == WorkbenchActionType::Exit;
		if (hasUnsavedChanges && actionLeavesDocument) {
			m_PendingAction = action;
			m_OpenUnsavedChangesPopup = true;
			return;
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
			case WorkbenchActionType::Exit:
				Application::GetInstance().RequestShutdown();
				return true;
            default:
                return true;
        }
    }

    void EditorLayer::OnUpdate() {
		ResolveEditorInput();
        if (m_Mode != EditorWorkbenchMode::WorkbenchShell || !m_WorkbenchReady || !m_SceneDocument.SceneRef) {
            return;
        }

		const auto& input = m_InteractionHost.Input();
		const Editor::EditorCameraInputState cameraInput{
			.MoveForward = input.GetActionValue("editor.camera.forward"),
			.MoveRight = input.GetActionValue("editor.camera.right"),
			.LookX = input.GetActionValue("editor.camera.look_x"),
			.LookY = input.GetActionValue("editor.camera.look_y"),
			.PanX = input.GetActionValue("editor.camera.pan_x"),
			.PanY = input.GetActionValue("editor.camera.pan_y"),
			.Zoom = input.GetActionValue("editor.camera.zoom")
		};
		if (m_EditorCameraController->Update(cameraInput)) {
			m_SceneCameraPoseDirty = true;
		}
		if (m_SceneCameraPoseDirty && std::chrono::steady_clock::now() - m_LastSceneCameraPoseSave >= std::chrono::seconds(1)) {
			PersistCurrentProjectSession();
		}
		const auto camera = m_EditorCameraController->BuildRenderCamera();
		const auto renderResult = Application::GetInstance().GetOperations().RenderSceneViewport(
			*m_SceneDocument.SceneRef,
			camera,
			&m_SceneRenderExtension);
        if (!renderResult.Succeeded()) {
            CaptureOperationResult(renderResult);
            m_WorkbenchReady = false;
        }
        m_SceneDocument.SceneRef->Update();
    }

	void EditorLayer::OnEvent(Event& event) {
		if (event.GetEventType() == EventType::WindowClose && (m_SceneDocument.Dirty || (m_AssetInspectorEditor && m_AssetInspectorEditor->HasDirtyEdit()))) {
			event.Handled = true;
			RequestWorkbenchAction({ WorkbenchActionType::Exit });
			return;
		}
	}

	void EditorLayer::OnDetach() {
		if (m_SceneCameraPoseDirty) {
			PersistCurrentProjectSession();
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

    void EditorLayer::ResolveEditorInput() {
		auto& input = m_InteractionHost.Input();
		input.Contexts().BeginFrame();
		const auto& snapshot = Application::GetInstance().GetInputSnapshot();
		if (snapshot.GetCaptureState().TextInput) input.Contexts().Activate("TextInput", 1000, true, false, true);
		if (m_IsModalOpen) input.Contexts().Activate("Modal", 900, true, true, true);
		if (m_IsSceneViewportFocused || m_IsSceneViewportHovered) input.Contexts().Activate("SceneViewport", 500, true, false, false);
		if (m_IsSceneViewportHovered) input.Contexts().Activate("SceneViewport", 500, false, true, false);
		if (m_ProjectPanel && (m_ProjectPanel->IsFocused() || m_ProjectPanel->IsHovered())) {
			input.Contexts().Activate("Project", 400, m_ProjectPanel->IsFocused(), m_ProjectPanel->IsHovered(), false);
		}
		if (m_HierarchyPanel && (m_HierarchyPanel->IsFocused() || m_HierarchyPanel->IsHovered())) {
			input.Contexts().Activate("Hierarchy", 400, m_HierarchyPanel->IsFocused(), m_HierarchyPanel->IsHovered(), false);
		}
		if (m_Inspector && (m_Inspector->IsFocused() || m_Inspector->IsHovered())) {
			input.Contexts().Activate("Inspector", 400, m_Inspector->IsFocused(), m_Inspector->IsHovered(), false);
		}
		if (m_Concole && (m_Concole->IsFocused() || m_Concole->IsHovered())) {
			input.Contexts().Activate("Console", 400, m_Concole->IsFocused(), m_Concole->IsHovered(), false);
		}
		(void)input.Resolve(snapshot);
	}

	bool EditorLayer::DrawCommandMenuItem(std::string_view commandId, std::string_view label) {
		auto& input = m_InteractionHost.Input();
		const auto* command = input.Commands().Find(commandId);
		if (!command) return false;
		const auto shortcut = input.Bindings().GetDisplayText(commandId);
		const auto displayName = label.empty() ? command->DisplayName : std::string(label);
		if (!ImGui::MenuItem(displayName.c_str(), shortcut.empty() ? nullptr : shortcut.c_str(), false, input.Commands().CanExecute(commandId))) return false;
		(void)input.Commands().Execute(commandId);
		return true;
    }

    void EditorLayer::OnUnsavedChangesPopup() {
        if (m_OpenUnsavedChangesPopup) {
			ImGui::OpenPopup("Unsaved Changes");
            m_OpenUnsavedChangesPopup = false;
        }

		if (ImGui::BeginPopupModal("Unsaved Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			const bool assetDirty = m_AssetInspectorEditor && m_AssetInspectorEditor->HasDirtyEdit();
			const bool sceneDirty = m_SceneDocument.IsLoaded() && m_SceneDocument.Dirty;
			ImGui::TextWrapped("The current editing context has unsaved changes. Apply and save before continuing?");
			if (assetDirty) ImGui::BulletText("Asset working copy has unapplied changes");
			if (sceneDirty) ImGui::BulletText("Scene document has unsaved changes");
            ImGui::Spacing();

			if (ImGui::Button("Apply, Save and Continue")) {
				AssetApplyState assetApplyState = AssetApplyState::ValidationFailed;
				if (assetDirty) (void)m_AssetInspectorEditor->Apply(&assetApplyState);
				const bool assetSaved = !assetDirty || IsAssetAuthoringDataSaved(assetApplyState);
				const bool sceneSaved = !sceneDirty || (assetSaved && SaveActiveSceneDocument());
				if (assetSaved && sceneSaved) {
					const auto action = m_PendingAction;
                    m_PendingAction = {};
                    ImGui::CloseCurrentPopup();
                    ExecuteWorkbenchAction(action);
                }
            }

            ImGui::SameLine();
			if (ImGui::Button("Discard and Continue")) {
				if (assetDirty) m_AssetInspectorEditor->Revert();
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
			m_AssetInspectorEditor->DrawModals();
            OnUnsavedChangesPopup();
            return;
        }

        OnDockingPanel();

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
			m_AssetInspectorEditor->DrawModals();
            return;
        }

        if (m_ShowProjectPanel && m_ProjectPanel) {
			m_ProjectPanel->SetSelectedAssetGuid(Selection::GetSelectedAssetGuid());
            m_ProjectPanel->OnGuiRender();
            if (const auto action = m_ProjectPanel->ConsumePendingAction()) {
                switch (action->Type) {
                    case ProjectPanelActionType::OpenScene:
                        RequestWorkbenchAction({ WorkbenchActionType::OpenScene, action->Path });
                        break;
					case ProjectPanelActionType::OpenSource: {
						ShaderDescriptor descriptor;
						const auto sourcePath = LoadShaderDescriptor(action->Path, descriptor).Succeeded() ? action->Path.parent_path() / descriptor.Source : action->Path;
						(void)HostLaunch::Open(sourcePath);
						break;
					}
                    case ProjectPanelActionType::RefreshProject:
                        if (m_ProjectSession.IsLoaded()) {
                            ProjectStatusReport status;
                            CaptureOperationResult(Application::GetInstance().GetOperations().CheckProjectStatus(m_ProjectSession.Context, &status));
                            if (m_LastOperationResult.Succeeded()) {
								m_AssetInspectorEditor->CheckExternalModification();
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
					case ProjectPanelActionType::ReimportPath:
						ReimportProjectAssets(action->Path);
						break;
					case ProjectPanelActionType::ReimportAll:
						if (m_ProjectSession.IsLoaded()) {
							ReimportProjectAssets(m_ProjectSession.Context.GetAssetRootPath());
						}
						break;
					case ProjectPanelActionType::SelectAsset:
						Selection::SelectAsset(action->Guid);
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
		m_AssetInspectorEditor->DrawModals();
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
				DrawCommandMenuItem("editor.project.resume");
				DrawCommandMenuItem("editor.project.close");
				DrawCommandMenuItem("editor.project.reset_session");
				DrawCommandMenuItem("editor.project.refresh");
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Scene")) {
				DrawCommandMenuItem("editor.scene.new");
				DrawCommandMenuItem("editor.scene.open");
				DrawCommandMenuItem("editor.scene.save", "Save Scene");
				DrawCommandMenuItem("editor.scene.save_as");
				DrawCommandMenuItem("editor.scene.validate");
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Edit")) {
                const auto undoLabel = m_InteractionHost.GetUndoLabel();
                const auto redoLabel = m_InteractionHost.GetRedoLabel();
                std::string undoTitle = undoLabel.empty() ? "Undo" : ("Undo " + undoLabel);
                std::string redoTitle = redoLabel.empty() ? "Redo" : ("Redo " + redoLabel);

				DrawCommandMenuItem("editor.undo", undoTitle);
				DrawCommandMenuItem("editor.redo", redoTitle);

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

		if (m_RequestNewScenePopup) {
			ImGui::OpenPopup("New Scene");
			m_RequestNewScenePopup = false;
		}
		if (m_RequestOpenScenePopup) {
			ImGui::OpenPopup("Open Scene");
			m_RequestOpenScenePopup = false;
		}
		if (m_RequestSaveSceneAsPopup) {
			ImGui::OpenPopup("Save Scene As");
			m_RequestSaveSceneAsPopup = false;
		}
		m_IsModalOpen = ImGui::IsPopupOpen("New Scene") || ImGui::IsPopupOpen("Open Scene") ||
			ImGui::IsPopupOpen("Save Scene As") || ImGui::IsPopupOpen("Unsaved Changes");

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

		OnUnsavedChangesPopup();
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
			m_ObjectIdRenderTarget->Resize((uint32_t)scenePanelSize.x, (uint32_t)scenePanelSize.y);
            m_SceneViewportSize = newViewportSize;
        }

        const ImVec2 viewportOrigin = ImGui::GetCursorScreenPos();
        ImGui::Image(m_RenderTarget->GetColorAttachmentView().NativeHandle,
            { m_SceneViewportSize.x , m_SceneViewportSize.y },
            { 0, 1 }, { 1, 0 });
		m_IsSceneViewportHovered = ImGui::IsItemHovered();
		m_IsSceneViewportFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        m_EditorCameraController->SetViewport(m_SceneViewportSize.x, m_SceneViewportSize.y);

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

        if (m_IsSceneViewportHovered && m_InteractionHost.Input().WasActionTriggered("editor.scene.pick") && !ImGuizmo::IsOver()) {
			const auto pointerPosition = Application::GetInstance().GetInputSnapshot().GetPointerPosition();
			const float localX = pointerPosition.x - viewportOrigin.x;
			const float localY = pointerPosition.y - viewportOrigin.y;
            const auto objectIdTexture = m_ObjectIdRenderTarget->GetColorAttachmentTexture();
            if (objectIdTexture) {
                const int maxX = static_cast<int>(objectIdTexture->GetWidth()) - 1;
                const int maxY = static_cast<int>(objectIdTexture->GetHeight()) - 1;
                const uint32_t pixelX = static_cast<uint32_t>(std::clamp(static_cast<int>(localX), 0, maxX));
                const uint32_t pixelY = static_cast<uint32_t>(std::clamp(maxY - static_cast<int>(localY), 0, maxY));
                const auto pixel = m_ObjectIdRenderTarget->ReadPixelRGBA8(0, pixelX, pixelY);
                const uint32_t encodedEntityId = DecodeObjectId(pixel);
                if (encodedEntityId == 0u) {
                    Selection::ClearSelection();
                } else {
                    Selection::SetSelection(m_SceneDocument.SceneRef->GetWorld().GetEntityByIndex(encodedEntityId - 1u));
                }
            }
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }
}
