#include "enginepch.h"
#include "ConsolePanel.h"

namespace HE {
    void ConcolePanel::OnGuiRender() {
        ImGui::Begin("Console");

        if (ImGui::Button("Clear Logs")) {
            Log::GetLogSink()->Clear();
        }

        ImGui::Separator();

        if (ImGui::BeginTabBar("ConsoleTabs")) {
            if (ImGui::BeginTabItem("Diagnostics")) {
                ImGui::BeginChild("DiagnosticsScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

                if (m_WorkbenchState) {
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

                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Logs")) {
                const auto& buffer = Log::GetLogSink()->GetBuffer();
                ImGui::BeginChild("LogScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

                for (const auto& line : buffer) {
                    ImVec4 color = LevelToColor(line.level);
                    ImGui::PushStyleColor(ImGuiCol_Text, color);
                    ImGui::TextUnformatted(line.message.c_str());
                    ImGui::PopStyleColor();
                }

                if (m_AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
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
}
