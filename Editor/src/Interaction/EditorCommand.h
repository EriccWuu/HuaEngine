#pragma once

#include <memory>
#include <string>

#include "HuaEngine/Core/ResultEnvelope.h"

namespace HE {
    class EditorWorkbenchState;
    struct ProjectSession;
    struct SceneDocument;

    struct EditorCommandContext {
        EditorWorkbenchState* WorkbenchState = nullptr;
        ProjectSession* ProjectSession = nullptr;
        SceneDocument* SceneDocument = nullptr;
    };

    class IEditorCommand {
    public:
        virtual ~IEditorCommand() = default;

        [[nodiscard]] virtual std::string GetLabel() const = 0;
        [[nodiscard]] virtual ResultEnvelope Execute(const EditorCommandContext& context) = 0;
        [[nodiscard]] virtual ResultEnvelope Undo(const EditorCommandContext& context) = 0;
    };

    using EditorCommandPtr = std::unique_ptr<IEditorCommand>;
}
