#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "imgui.h"

namespace HE {
    struct ShortcutBinding {
        std::string CommandId;
        std::string DisplayName;
        ImGuiKeyChord Chord = ImGuiKey_None;
        std::string Shortcut;
        std::function<bool()> IsEnabled;
        std::function<void()> Trigger;
    };

    class ShortcutRegistry {
    public:
        void Clear();
        void Register(ShortcutBinding binding);
        void DispatchTriggered() const;

        [[nodiscard]] const ShortcutBinding* Find(std::string_view commandId) const;

    private:
        std::unordered_map<std::string, ShortcutBinding> m_Bindings;
    };
}
