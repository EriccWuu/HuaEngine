#pragma once

#include <optional>
#include <span>
#include <string>

#include "HuaEngine/Core/Log.h"
#include "Workbench/EditorWorkbenchState.h"
#include "imgui.h"

namespace HE {
	[[nodiscard]] std::string BuildConsoleLogText(std::span<const LogSink::LogLine> lines);

	class ConcolePanel {
    public:
        ConcolePanel() = default;
        ~ConcolePanel() = default;

        void OnGuiRender();
        void SetWorkbenchState(const EditorWorkbenchState* state) { m_WorkbenchState = state; }
        void SetAutoScroll(bool enable) { m_AutoScroll = enable; }
		[[nodiscard]] bool IsFocused() const { return m_IsFocused; }
		[[nodiscard]] bool IsHovered() const { return m_IsHovered; }
		[[nodiscard]] bool HasSelectedLog() const;
		void CopySelectedLog() const;
		void CopyAllLogs() const;

    private:
        bool m_AutoScroll = true;
        const EditorWorkbenchState* m_WorkbenchState = nullptr;
		bool m_IsFocused = false;
		bool m_IsHovered = false;
		std::optional<size_t> m_SelectedLogIndex;

        ImVec4 LevelToColor(spdlog::level::level_enum level);
        ImVec4 SeverityToColor(DiagnosticSeverity severity);
        bool IsScrollNearBottom() const;
	};
}
