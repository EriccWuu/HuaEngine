#include "enginepch.h"
#include "Interaction/ShortcutRegistry.h"

namespace HE {
    namespace {
        bool IsChordPressed(ImGuiKeyChord chord) {
            if (chord == ImGuiKey_None) {
                return false;
            }

            ImGuiIO& io = ImGui::GetIO();
            const bool wantsCtrl = (chord & ImGuiMod_Ctrl) != 0;
            const bool wantsShift = (chord & ImGuiMod_Shift) != 0;
            const bool wantsAlt = (chord & ImGuiMod_Alt) != 0;
            const bool wantsSuper = (chord & ImGuiMod_Super) != 0;

            if (io.KeyCtrl != wantsCtrl || io.KeyShift != wantsShift || io.KeyAlt != wantsAlt || io.KeySuper != wantsSuper) {
                return false;
            }

            const auto key = static_cast<ImGuiKey>(chord & ~ImGuiMod_Mask_);
            return key != ImGuiKey_None && ImGui::IsKeyPressed(key, false);
        }
    }

    void ShortcutRegistry::Clear() {
        m_Bindings.clear();
    }

    void ShortcutRegistry::Register(ShortcutBinding binding) {
        m_Bindings[binding.CommandId] = std::move(binding);
    }

    void ShortcutRegistry::DispatchTriggered() const {
        if (ImGui::GetIO().WantTextInput) {
            return;
        }

        for (const auto& [commandId, binding] : m_Bindings) {
            (void)commandId;
            const bool enabled = binding.IsEnabled ? binding.IsEnabled() : true;
            if (!enabled || !binding.Trigger) {
                continue;
            }

            if (IsChordPressed(binding.Chord)) {
                binding.Trigger();
            }
        }
    }

    const ShortcutBinding* ShortcutRegistry::Find(std::string_view commandId) const {
        const auto it = m_Bindings.find(std::string(commandId));
        return it != m_Bindings.end() ? &it->second : nullptr;
    }
}
