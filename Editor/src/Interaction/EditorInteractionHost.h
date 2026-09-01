#pragma once

#include <functional>
#include <string_view>

#include "Interaction/ContextMenuRegistry.h"
#include "Interaction/EditorCommand.h"
#include "Interaction/DragDropIntentRegistry.h"
#include "Interaction/EditorCommandRouter.h"
#include "Interaction/EditorCommandStack.h"
#include "Input/EditorInputService.h"

namespace HE {
    class EditorWorkbenchState;
    struct ProjectSession;
    struct SceneDocument;

    class EditorInteractionHost {
    public:
        void Reset();
        void Bind(EditorWorkbenchState* workbenchState, ProjectSession* projectSession, SceneDocument* sceneDocument);
        void SetStateChangedCallback(std::function<void()> callback);

        [[nodiscard]] bool HasActiveProject() const;
        [[nodiscard]] bool HasActiveScene() const;
        [[nodiscard]] ResultEnvelope ExecuteCommand(EditorCommandPtr command);
        [[nodiscard]] ResultEnvelope Undo();
        [[nodiscard]] ResultEnvelope Redo();
        [[nodiscard]] ResultEnvelope MarkExternalSceneMutation(std::string_view operation, std::string_view target, std::string_view summary);
        void ResetCommandHistory(bool markSaved);
        void MarkSaved();
        [[nodiscard]] bool CanUndo() const;
        [[nodiscard]] bool CanRedo() const;
        [[nodiscard]] bool IsSceneDirty() const;
        [[nodiscard]] std::string GetUndoLabel() const;
        [[nodiscard]] std::string GetRedoLabel() const;

        [[nodiscard]] EditorWorkbenchState* GetWorkbenchState() const { return m_WorkbenchState; }
        [[nodiscard]] ProjectSession* GetProjectSession() const { return m_ProjectSession; }
        [[nodiscard]] SceneDocument* GetSceneDocument() const { return m_SceneDocument; }

        [[nodiscard]] EditorCommandRouter& Commands() { return m_CommandRouter; }
        [[nodiscard]] ContextMenuRegistry& ContextMenus() { return m_ContextMenus; }
        [[nodiscard]] Editor::EditorInputService& Input() { return m_Input; }
        [[nodiscard]] DragDropIntentRegistry& DragDrop() { return m_DragDrop; }

        [[nodiscard]] const EditorCommandRouter& Commands() const { return m_CommandRouter; }
        [[nodiscard]] const ContextMenuRegistry& ContextMenus() const { return m_ContextMenus; }
        [[nodiscard]] const Editor::EditorInputService& Input() const { return m_Input; }
        [[nodiscard]] const DragDropIntentRegistry& DragDrop() const { return m_DragDrop; }

    private:
        EditorWorkbenchState* m_WorkbenchState = nullptr;
        ProjectSession* m_ProjectSession = nullptr;
        SceneDocument* m_SceneDocument = nullptr;
        EditorCommandRouter m_CommandRouter;
        ContextMenuRegistry m_ContextMenus;
        Editor::EditorInputService m_Input;
        DragDropIntentRegistry m_DragDrop;
        EditorCommandStack m_CommandStack;
        std::function<void()> m_StateChangedCallback;

        [[nodiscard]] EditorCommandContext BuildCommandContext() const;
        void SyncSceneDocumentDirtyState();
        void NotifyStateChanged() const;
    };
}
