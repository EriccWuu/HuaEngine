#include "enginepch.h"
#include "Interaction/EditorInteractionHost.h"

#include "Workbench/EditorWorkbenchState.h"
#include "Workbench/ProjectSession.h"
#include "Workbench/SceneDocument.h"

namespace HE {
    void EditorInteractionHost::Reset() {
        m_WorkbenchState = nullptr;
        m_ProjectSession = nullptr;
        m_SceneDocument = nullptr;
        m_CommandRouter.Reset();
        m_ContextMenus.Clear();
        m_Shortcuts.Clear();
        m_DragDrop.Clear();
        m_CommandStack.Clear();
    }

    void EditorInteractionHost::Bind(EditorWorkbenchState* workbenchState, ProjectSession* projectSession, SceneDocument* sceneDocument) {
        m_WorkbenchState = workbenchState;
        m_ProjectSession = projectSession;
        m_SceneDocument = sceneDocument;
        SyncSceneDocumentDirtyState();
    }

    void EditorInteractionHost::SetStateChangedCallback(std::function<void()> callback) {
        m_StateChangedCallback = std::move(callback);
    }

    bool EditorInteractionHost::HasActiveProject() const {
        return m_ProjectSession != nullptr && m_ProjectSession->IsLoaded();
    }

    bool EditorInteractionHost::HasActiveScene() const {
        return m_SceneDocument != nullptr && m_SceneDocument->IsLoaded();
    }

    ResultEnvelope EditorInteractionHost::ExecuteCommand(EditorCommandPtr command) {
        if (!HasActiveScene()) {
            auto result = ResultEnvelope::Failure("editor.command.execute", "editor.history", "An active scene document is required before executing an editor command");
            result.AddDetail({
                DiagnosticSeverity::Error,
                "editor.command.execute.scene_missing",
                "Open or create a scene document before running editor commands",
                {}
            });
            return result;
        }

        auto result = m_CommandStack.Execute(std::move(command), BuildCommandContext());
        SyncSceneDocumentDirtyState();
        NotifyStateChanged();
        return result;
    }

    ResultEnvelope EditorInteractionHost::Undo() {
        auto result = m_CommandStack.Undo(BuildCommandContext());
        SyncSceneDocumentDirtyState();
        NotifyStateChanged();
        return result;
    }

    ResultEnvelope EditorInteractionHost::Redo() {
        auto result = m_CommandStack.Redo(BuildCommandContext());
        SyncSceneDocumentDirtyState();
        NotifyStateChanged();
        return result;
    }

    ResultEnvelope EditorInteractionHost::MarkExternalSceneMutation(std::string_view operation, std::string_view target, std::string_view summary) {
        if (!HasActiveScene()) {
            auto result = ResultEnvelope::Failure(std::string(operation), std::string(target), "An active scene document is required before marking an editor mutation");
            result.AddDetail({
                DiagnosticSeverity::Error,
                "editor.command.external_mutation.scene_missing",
                "The editor interaction host does not currently have an active scene document",
                {}
            });
            return result;
        }

        m_CommandStack.MarkExternalMutation();
        SyncSceneDocumentDirtyState();
        NotifyStateChanged();
        return ResultEnvelope::Success(std::string(operation), std::string(target), std::string(summary));
    }

    void EditorInteractionHost::ResetCommandHistory(bool markSaved) {
        m_CommandStack.Clear();
        if (markSaved) {
            m_CommandStack.MarkSaved();
        }
        SyncSceneDocumentDirtyState();
        NotifyStateChanged();
    }

    void EditorInteractionHost::MarkSaved() {
        m_CommandStack.MarkSaved();
        SyncSceneDocumentDirtyState();
        NotifyStateChanged();
    }

    bool EditorInteractionHost::CanUndo() const {
        return m_CommandStack.CanUndo();
    }

    bool EditorInteractionHost::CanRedo() const {
        return m_CommandStack.CanRedo();
    }

    bool EditorInteractionHost::IsSceneDirty() const {
        return m_CommandStack.IsDirty();
    }

    std::string EditorInteractionHost::GetUndoLabel() const {
        return m_CommandStack.GetUndoLabel();
    }

    std::string EditorInteractionHost::GetRedoLabel() const {
        return m_CommandStack.GetRedoLabel();
    }

    EditorCommandContext EditorInteractionHost::BuildCommandContext() const {
        return {
            .WorkbenchState = m_WorkbenchState,
            .ProjectSession = m_ProjectSession,
            .SceneDocument = m_SceneDocument
        };
    }

    void EditorInteractionHost::SyncSceneDocumentDirtyState() {
        if (m_SceneDocument) {
            m_SceneDocument->ApplyDirtyState(m_CommandStack.IsDirty());
        }
    }

    void EditorInteractionHost::NotifyStateChanged() const {
        if (m_StateChangedCallback) {
            m_StateChangedCallback();
        }
    }
}
