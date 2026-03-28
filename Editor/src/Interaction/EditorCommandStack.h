#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Interaction/EditorCommand.h"

namespace HE {
    class EditorCommandStack {
    public:
        void Clear();

        [[nodiscard]] ResultEnvelope Execute(EditorCommandPtr command, const EditorCommandContext& context);
        [[nodiscard]] ResultEnvelope Undo(const EditorCommandContext& context);
        [[nodiscard]] ResultEnvelope Redo(const EditorCommandContext& context);

        void MarkSaved();
        void MarkExternalMutation();

        [[nodiscard]] bool CanUndo() const;
        [[nodiscard]] bool CanRedo() const;
        [[nodiscard]] bool IsDirty() const;
        [[nodiscard]] std::string GetUndoLabel() const;
        [[nodiscard]] std::string GetRedoLabel() const;
        [[nodiscard]] uint64_t GetRevision() const { return m_Revision; }
        [[nodiscard]] uint64_t GetSavedRevision() const { return m_SavedRevision; }

    private:
        std::vector<EditorCommandPtr> m_UndoStack;
        std::vector<EditorCommandPtr> m_RedoStack;
        uint64_t m_Revision = 0;
        uint64_t m_SavedRevision = 0;
    };
}
