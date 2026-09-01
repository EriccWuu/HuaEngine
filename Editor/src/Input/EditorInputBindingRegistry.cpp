#include "EditorInputBindingRegistry.h"

#include <algorithm>

namespace HE::Editor {
	namespace {
		std::string FormatKey(const InputControl& control) {
			if (control.Device == InputDeviceType::Mouse) return "Mouse" + std::to_string(control.Code + 1);
			if (control.Code >= Key::A && control.Code <= Key::Z) return std::string(1, static_cast<char>('A' + control.Code - Key::A));
			if (control.Code >= Key::D0 && control.Code <= Key::D9) return std::string(1, static_cast<char>('0' + control.Code - Key::D0));
			switch (control.Code) {
			case Key::Space: return "Space";
			case Key::Enter: return "Enter";
			case Key::Escape: return "Esc";
			case Key::Delete: return "Delete";
			case Key::Backspace: return "Backspace";
			case Key::Tab: return "Tab";
			default: return "Key" + std::to_string(control.Code);
			}
		}

		std::string FormatGesture(const InputGesture& gesture) {
			std::string result;
			if (HasModifier(gesture.Modifiers, InputModifiers::Control)) result += "Ctrl+";
			if (HasModifier(gesture.Modifiers, InputModifiers::Shift)) result += "Shift+";
			if (HasModifier(gesture.Modifiers, InputModifiers::Alt)) result += "Alt+";
			if (HasModifier(gesture.Modifiers, InputModifiers::Super)) result += "Super+";
			result += FormatKey(gesture.Primary);
			return result;
		}
	}

	ResultEnvelope EditorInputBindingRegistry::RegisterDefaultCommand(EditorCommandBinding binding) {
		if (binding.Id.empty() || binding.CommandId.empty() || binding.ContextId.empty()) {
			return ResultEnvelope::Failure("editor.input.binding.register", binding.Id, "Command binding is incomplete");
		}
		if (std::ranges::any_of(m_Commands, [&](const auto& existing) { return existing.Id == binding.Id; })) {
			return ResultEnvelope::Failure("editor.input.binding.register", binding.Id, "Binding id is already registered");
		}
		const auto id = binding.Id;
		m_Commands.emplace_back(std::move(binding));
		return ResultEnvelope::Success("editor.input.binding.register", id, "Command binding registered");
	}

	ResultEnvelope EditorInputBindingRegistry::RegisterDefaultAction(EditorActionBinding binding) {
		if (binding.Id.empty() || binding.ActionId.empty() || binding.ContextId.empty()) {
			return ResultEnvelope::Failure("editor.input.action.register", binding.Id, "Action binding is incomplete");
		}
		if (std::ranges::any_of(m_Actions, [&](const auto& existing) { return existing.Id == binding.Id; })) {
			return ResultEnvelope::Failure("editor.input.action.register", binding.Id, "Action binding id is already registered");
		}
		const auto id = binding.Id;
		m_Actions.emplace_back(std::move(binding));
		return ResultEnvelope::Success("editor.input.action.register", id, "Action binding registered");
	}

	ResultEnvelope EditorInputBindingRegistry::SetOverrides(std::vector<EditorInputBindingOverride> overrides) {
		m_Overrides = std::move(overrides);
		return ResultEnvelope::Success("editor.input.binding.override", "user", "Binding overrides applied");
	}

	void EditorInputBindingRegistry::Clear() {
		m_Commands.clear();
		m_Actions.clear();
		m_Overrides.clear();
	}

	std::vector<EditorCommandBinding> EditorInputBindingRegistry::GetEffectiveCommandBindings() const {
		auto effective = m_Commands;
		for (auto& binding : effective) {
			const auto overrideIterator = std::ranges::find_if(m_Overrides, [&](const auto& overrideBinding) {
				return overrideBinding.CommandId == binding.CommandId;
			});
			if (overrideIterator == m_Overrides.end()) continue;
			binding.ContextId = overrideIterator->ContextId;
			binding.Gesture = overrideIterator->Gesture;
		}
		return effective;
	}

	std::string EditorInputBindingRegistry::GetDisplayText(std::string_view commandId) const {
		const auto effective = GetEffectiveCommandBindings();
		const auto iterator = std::ranges::find_if(effective, [&](const auto& binding) { return binding.CommandId == commandId; });
		return iterator == effective.end() ? std::string{} : FormatGesture(iterator->Gesture);
	}
}
