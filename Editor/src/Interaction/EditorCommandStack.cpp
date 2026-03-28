#include "enginepch.h"
#include "Interaction/EditorCommandStack.h"

namespace HE {
    namespace {
        ResultEnvelope MakeHistoryUnavailableResult(std::string operation, std::string summary) {
            auto result = ResultEnvelope::Failure(std::move(operation), "editor.history", std::move(summary));
            result.AddDetail({
                DiagnosticSeverity::Info,
                "editor.history.empty",
                "The editor command history does not currently contain a matching entry",
                {}
            });
            return result;
        }
    }

    void EditorCommandStack::Clear() {
        m_UndoStack.clear();
        m_RedoStack.clear();
        m_Revision = 0;
        m_SavedRevision = 0;
    }

    ResultEnvelope EditorCommandStack::Execute(EditorCommandPtr command, const EditorCommandContext& context) {
        if (!command) {
            auto result = ResultEnvelope::Failure("editor.command.execute", "editor.history", "Cannot execute an empty editor command");
            result.AddDetail({
                DiagnosticSeverity::Error,
                "editor.command.execute.missing_command",
                "A null editor command was passed into the command stack",
                {}
            });
            return result;
        }

        auto result = command->Execute(context);
        if (!result.Succeeded()) {
            return result;
        }

        m_RedoStack.clear();
        m_UndoStack.push_back(std::move(command));
        ++m_Revision;
        return result;
    }

    ResultEnvelope EditorCommandStack::Undo(const EditorCommandContext& context) {
        if (m_UndoStack.empty()) {
            return MakeHistoryUnavailableResult("editor.command.undo", "No command is available to undo");
        }

        auto command = std::move(m_UndoStack.back());
        m_UndoStack.pop_back();

        auto result = command->Undo(context);
        if (!result.Succeeded()) {
            m_UndoStack.push_back(std::move(command));
            return result;
        }

        m_RedoStack.push_back(std::move(command));
        ++m_Revision;
        return result;
    }

    ResultEnvelope EditorCommandStack::Redo(const EditorCommandContext& context) {
        if (m_RedoStack.empty()) {
            return MakeHistoryUnavailableResult("editor.command.redo", "No command is available to redo");
        }

        auto command = std::move(m_RedoStack.back());
        m_RedoStack.pop_back();

        auto result = command->Execute(context);
        if (!result.Succeeded()) {
            m_RedoStack.push_back(std::move(command));
            return result;
        }

        m_UndoStack.push_back(std::move(command));
        ++m_Revision;
        return result;
    }

    void EditorCommandStack::MarkSaved() {
        m_SavedRevision = m_Revision;
    }

    void EditorCommandStack::MarkExternalMutation() {
        m_RedoStack.clear();
        ++m_Revision;
    }

    bool EditorCommandStack::CanUndo() const {
        return !m_UndoStack.empty();
    }

    bool EditorCommandStack::CanRedo() const {
        return !m_RedoStack.empty();
    }

    bool EditorCommandStack::IsDirty() const {
        return m_Revision != m_SavedRevision;
    }

    std::string EditorCommandStack::GetUndoLabel() const {
        return m_UndoStack.empty() ? std::string() : m_UndoStack.back()->GetLabel();
    }

    std::string EditorCommandStack::GetRedoLabel() const {
        return m_RedoStack.empty() ? std::string() : m_RedoStack.back()->GetLabel();
    }
}
