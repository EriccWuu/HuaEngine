#pragma once

#include "HuaEngine/Core/Log.h"
#include "imgui.h"

namespace HE {
	class ConcolePanel {
    public:
        ConcolePanel() = default;
        ~ConcolePanel() = default;

        void OnGuiRender();
        void SetAutoScroll(bool enable) { m_AutoScroll = enable; }

    private:
        bool m_AutoScroll = true;

        ImVec4 LevelToColor(spdlog::level::level_enum level);
	};
}