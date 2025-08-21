#include "enginepch.h"
#include "ConsolePanel.h"

namespace HE {
    void ConcolePanel::OnGuiRender() {
        ImGui::Begin("Console");

        if (ImGui::Button("Clear")) {
            Log::GetLogSink()->Clear();
        }

        ImGui::Separator();

        const auto& buffer = Log::GetLogSink()->GetBuffer();

        ImGui::BeginChild("LogScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

        for (const auto& line : buffer) {
            ImVec4 color = LevelToColor(line.level);
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextUnformatted(line.message.c_str());
            ImGui::PopStyleColor();
        }

        if (m_AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);

        ImGui::EndChild();
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
}