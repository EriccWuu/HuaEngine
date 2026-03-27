#pragma once

#include "HuaEngine/Core/Log.h"
#include "Workbench/EditorWorkbenchState.h"
#include "imgui.h"

namespace HE {
	class ConcolePanel {
    public:
        ConcolePanel() = default;
        ~ConcolePanel() = default;

        void OnGuiRender();
        void SetWorkbenchState(const EditorWorkbenchState* state) { m_WorkbenchState = state; }
        void SetAutoScroll(bool enable) { m_AutoScroll = enable; }

    private:
        bool m_AutoScroll = true;
        const EditorWorkbenchState* m_WorkbenchState = nullptr;

        ImVec4 LevelToColor(spdlog::level::level_enum level);
        ImVec4 SeverityToColor(DiagnosticSeverity severity);
	};
}
