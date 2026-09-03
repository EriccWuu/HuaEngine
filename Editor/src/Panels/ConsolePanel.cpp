#include "enginepch.h"
#include "ConsolePanel.h"

namespace HE {
	bool ConsoleLogFilter::Allows(spdlog::level::level_enum level) const {
		switch (level) {
		case spdlog::level::warn: return Warning;
		case spdlog::level::err:
		case spdlog::level::critical: return Error;
		default: return Info;
		}
	}

	std::string BuildConsoleLogText(
		std::span<const LogSink::LogLine> lines,
		const ConsoleLogFilter& filter) {
		std::string text;
		for (const auto& line : lines) {
			if (!filter.Allows(line.level)) continue;
			if (!text.empty()) text.push_back('\n');
			text += line.message;
		}
		return text;
	}

	bool ConcolePanel::HasSelectedLog() const {
		const auto& sink = Log::GetLogSink();
		return sink && m_SelectedLogIndex && *m_SelectedLogIndex < sink->GetBuffer().size()
			&& m_LogFilter.Allows(sink->GetBuffer()[*m_SelectedLogIndex].level);
	}

	void ConcolePanel::CopySelectedLog() const {
		if (!HasSelectedLog()) return;
		ImGui::SetClipboardText(Log::GetLogSink()->GetBuffer()[*m_SelectedLogIndex].message.c_str());
	}

	void ConcolePanel::CopyAllLogs() const {
		const auto& sink = Log::GetLogSink();
		if (!sink) return;
		const auto text = BuildConsoleLogText(sink->GetBuffer(), m_LogFilter);
		ImGui::SetClipboardText(text.c_str());
	}

    void ConcolePanel::OnGuiRender() {
        ImGui::Begin("Console");
		m_IsFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		m_IsHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

        if (ImGui::Button("Clear Logs")) {
            Log::GetLogSink()->Clear();
			m_SelectedLogIndex.reset();
        }
		ImGui::SameLine();
		ImGui::BeginDisabled(!HasSelectedLog());
		if (ImGui::Button("Copy")) CopySelectedLog();
		ImGui::EndDisabled();
		ImGui::SameLine();
		const auto& logSink = Log::GetLogSink();
		ImGui::BeginDisabled(!logSink || logSink->GetBuffer().empty());
		if (ImGui::Button("Copy All")) CopyAllLogs();
		ImGui::EndDisabled();
		if (logSink) {
			size_t infoCount = 0;
			size_t warningCount = 0;
			size_t errorCount = 0;
			for (const auto& line : logSink->GetBuffer()) {
				if (line.level == spdlog::level::warn) ++warningCount;
				else if (line.level == spdlog::level::err || line.level == spdlog::level::critical) ++errorCount;
				else ++infoCount;
			}

			const auto infoLabel = "Info (" + std::to_string(infoCount) + ")##ConsoleInfoFilter";
			const auto warningLabel = "Warning (" + std::to_string(warningCount) + ")##ConsoleWarningFilter";
			const auto errorLabel = "Error (" + std::to_string(errorCount) + ")##ConsoleErrorFilter";
			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Text, LevelToColor(spdlog::level::info));
			ImGui::Checkbox(infoLabel.c_str(), &m_LogFilter.Info);
			ImGui::PopStyleColor();
			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Text, LevelToColor(spdlog::level::warn));
			ImGui::Checkbox(warningLabel.c_str(), &m_LogFilter.Warning);
			ImGui::PopStyleColor();
			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Text, LevelToColor(spdlog::level::err));
			ImGui::Checkbox(errorLabel.c_str(), &m_LogFilter.Error);
			ImGui::PopStyleColor();
		}

        ImGui::Separator();

        if (ImGui::BeginTabBar("ConsoleTabs")) {
            if (ImGui::BeginTabItem("Logs")) {
				const auto& buffer = logSink->GetBuffer();
				if (m_SelectedLogIndex && *m_SelectedLogIndex >= buffer.size()) m_SelectedLogIndex.reset();
                ImGui::BeginChild("LogScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
                const bool wasAtBottom = IsScrollNearBottom();

				for (size_t index = 0; index < buffer.size(); ++index) {
					const auto& line = buffer[index];
					if (!m_LogFilter.Allows(line.level)) continue;
					ImGui::PushID(static_cast<int>(index));
					const auto color = LevelToColor(line.level);
					const float rowHeight = ImGui::GetTextLineHeightWithSpacing();
					const float rowWidth = std::max(
						ImGui::GetContentRegionAvail().x,
						ImGui::CalcTextSize(line.message.c_str()).x + ImGui::GetStyle().FramePadding.x * 2.0f);
					if (ImGui::Selectable("##LogLine", m_SelectedLogIndex == index, ImGuiSelectableFlags_None, { rowWidth, rowHeight })) {
						m_SelectedLogIndex = index;
					}
					const auto rowMin = ImGui::GetItemRectMin();
					const float textOffsetY = (rowHeight - ImGui::GetTextLineHeight()) * 0.5f;
					ImGui::GetWindowDrawList()->AddText(
						{ rowMin.x + ImGui::GetStyle().FramePadding.x, rowMin.y + textOffsetY },
						ImGui::ColorConvertFloat4ToU32(color),
						line.message.c_str());
					if (ImGui::BeginPopupContextItem("LogContext")) {
						if (ImGui::MenuItem("Copy")) {
							m_SelectedLogIndex = index;
							CopySelectedLog();
						}
						if (ImGui::MenuItem("Copy All")) CopyAllLogs();
						ImGui::EndPopup();
					}
					ImGui::PopID();
                }

                if (m_AutoScroll && wasAtBottom) {
                    ImGui::SetScrollHereY(1.0f);
                }

                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Diagnostics")) {
                ImGui::BeginChild("DiagnosticsScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
                const bool wasAtBottom = IsScrollNearBottom();

                if (m_WorkbenchState) {
                    if (const auto* session = m_WorkbenchState->GetProjectSessionSummary()) {
                        ImGui::Text("Project: %s", session->ProjectName.c_str());
                        ImGui::TextWrapped("Root: %s", session->RootPath.c_str());
                    }

                    if (const auto* scene = m_WorkbenchState->GetSceneDocumentSummary()) {
                        ImGui::Text("Scene: %s%s", scene->DisplayName.c_str(), scene->Dirty ? "*" : "");
                        if (!scene->ScenePath.empty()) {
                            ImGui::TextWrapped("Scene Path: %s", scene->ScenePath.c_str());
                        }
                    }

                    if (m_WorkbenchState->GetProjectSessionSummary() || m_WorkbenchState->GetSceneDocumentSummary()) {
                        ImGui::Separator();
                    }

                    const auto& events = m_WorkbenchState->GetEventHistory();
                    if (events.empty()) {
                        ImGui::TextUnformatted("No formal workbench diagnostics captured yet.");
                    }
                    else {
                        for (auto it = events.rbegin(); it != events.rend(); ++it) {
                            ImGui::SeparatorText(it->Source.empty() ? it->Result.Operation.c_str() : it->Source.c_str());
                            ImGui::Text("Status: %s", ToString(it->Result.Status).data());
                            ImGui::TextWrapped("%s", it->Result.Summary.c_str());
                            for (const auto& detail : it->Result.Details) {
                                ImGui::PushStyleColor(ImGuiCol_Text, SeverityToColor(detail.Severity));
                                ImGui::BulletText("[%s] %s", detail.Code.c_str(), detail.Message.c_str());
                                ImGui::PopStyleColor();
                            }
                        }
                    }
                }
                else {
                    ImGui::TextUnformatted("Workbench state is not connected.");
                }

                if (m_AutoScroll && wasAtBottom) {
                    ImGui::SetScrollHereY(1.0f);
                }

                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
        ImGui::End();
    }

    ImVec4 ConcolePanel::LevelToColor(spdlog::level::level_enum level) {
        switch (level) {
            case spdlog::level::trace: return { 0.5f, 0.5f, 0.5f, 1.0f };
            case spdlog::level::debug: return { 0.6f, 0.8f, 1.0f, 1.0f };
            case spdlog::level::info:  return { 1.0f, 1.0f, 1.0f, 1.0f };
            case spdlog::level::warn:  return { 1.0f, 1.0f, 0.4f, 1.0f };
            case spdlog::level::err:   return { 1.0f, 0.4f, 0.4f, 1.0f };
            case spdlog::level::critical: return { 1.0f, 0.0f, 0.0f, 1.0f };
            default: return { 1.0f, 1.0f, 1.0f, 1.0f };
            }
    }

    ImVec4 ConcolePanel::SeverityToColor(DiagnosticSeverity severity) {
        switch (severity) {
            case DiagnosticSeverity::Info: return { 0.7f, 0.85f, 1.0f, 1.0f };
            case DiagnosticSeverity::Warning: return { 1.0f, 0.85f, 0.3f, 1.0f };
            case DiagnosticSeverity::Error: return { 1.0f, 0.45f, 0.45f, 1.0f };
            default: return { 1.0f, 1.0f, 1.0f, 1.0f };
        }
    }

    bool ConcolePanel::IsScrollNearBottom() const {
        constexpr float kScrollSnapThreshold = 2.0f;
        return ImGui::GetScrollMaxY() - ImGui::GetScrollY() <= kScrollSnapThreshold;
    }
}
