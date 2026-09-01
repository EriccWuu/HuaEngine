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

		bool IsValidGesture(const InputGesture& gesture) {
			const auto modifierBits = static_cast<uint8_t>(gesture.Modifiers);
			constexpr auto validModifierBits = static_cast<uint8_t>(InputModifiers::Shift) |
				static_cast<uint8_t>(InputModifiers::Control) |
				static_cast<uint8_t>(InputModifiers::Alt) |
				static_cast<uint8_t>(InputModifiers::Super);
			if ((modifierBits & ~validModifierBits) != 0 || gesture.Trigger > InputTrigger::Scrolled) return false;
			if (gesture.Primary.Device == InputDeviceType::Keyboard) return gesture.Primary.Code <= Key::Menu;
			if (gesture.Primary.Device == InputDeviceType::Mouse) return gesture.Primary.Code <= Mouse::ButtonLast;
			return false;
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
		m_Overrides.clear();
		auto result = ResultEnvelope::Success("editor.input.binding.override", "user", "Binding overrides applied");
		for (auto& overrideBinding : overrides) {
			const bool knownCommand = std::ranges::any_of(m_Commands, [&](const auto& binding) { return binding.CommandId == overrideBinding.CommandId; });
			if (!knownCommand || overrideBinding.ContextId.empty() || !IsValidGesture(overrideBinding.Gesture)) {
				result.AddDetail({ DiagnosticSeverity::Warning, "editor.input.binding.override_invalid", "Invalid binding override was ignored", overrideBinding.CommandId });
				continue;
			}
			ResetOverride(overrideBinding.CommandId);
			m_Overrides.emplace_back(std::move(overrideBinding));
		}
		return result;
	}

	ResultEnvelope EditorInputBindingRegistry::SetOverride(EditorInputBindingOverride overrideBinding) {
		return SetOverrides([&]() {
			auto overrides = m_Overrides;
			overrides.emplace_back(std::move(overrideBinding));
			return overrides;
		}());
	}

	void EditorInputBindingRegistry::ResetOverride(std::string_view commandId) {
		std::erase_if(m_Overrides, [&](const auto& binding) { return binding.CommandId == commandId; });
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

	std::vector<std::string> EditorInputBindingRegistry::FindConflicts(std::string_view contextId, const InputGesture& gesture) const {
		std::vector<std::string> conflicts;
		for (const auto& binding : GetEffectiveCommandBindings()) {
			if (binding.ContextId == contextId && binding.Gesture == gesture) conflicts.emplace_back(binding.CommandId);
		}
		return conflicts;
	}
}
