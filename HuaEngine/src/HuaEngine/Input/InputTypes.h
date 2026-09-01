#pragma once

#include <cstdint>
#include <functional>

#include <glm/vec2.hpp>

#include "HuaEngine/Core/KeyCodes.h"
#include "HuaEngine/Core/MouseCodes.h"

namespace HE {
	enum class InputDeviceType : uint8_t {
		Keyboard,
		Mouse
	};

	enum class InputPhase : uint8_t {
		Pressed,
		Released,
		Repeated,
		Moved,
		Scrolled,
		Text
	};

	enum class InputModifiers : uint8_t {
		None = 0,
		Shift = 1 << 0,
		Control = 1 << 1,
		Alt = 1 << 2,
		Super = 1 << 3
	};

	enum class InputTrigger : uint8_t {
		Pressed,
		Released,
		Repeated,
		Held,
		DoublePressed,
		Scrolled
	};

	struct InputCaptureState {
		bool Keyboard = false;
		bool Pointer = false;
		bool TextInput = false;
	};

	constexpr InputModifiers operator|(InputModifiers lhs, InputModifiers rhs) {
		return static_cast<InputModifiers>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
	}

	constexpr InputModifiers operator&(InputModifiers lhs, InputModifiers rhs) {
		return static_cast<InputModifiers>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
	}

	constexpr bool HasModifier(InputModifiers value, InputModifiers modifier) {
		return (value & modifier) != InputModifiers::None;
	}

	struct InputControl {
		InputDeviceType Device = InputDeviceType::Keyboard;
		uint16_t Code = 0;

		[[nodiscard]] bool operator==(const InputControl&) const = default;
	};

	struct InputControlHash {
		[[nodiscard]] size_t operator()(const InputControl& control) const noexcept {
			return (static_cast<size_t>(control.Device) << 16) | control.Code;
		}
	};

	struct InputGesture {
		InputControl Primary;
		InputModifiers Modifiers = InputModifiers::None;
		InputTrigger Trigger = InputTrigger::Pressed;
		bool ExactModifiers = true;

		[[nodiscard]] bool operator==(const InputGesture&) const = default;
	};

	[[nodiscard]] constexpr InputControl KeyboardControl(KeyCode key) {
		return { InputDeviceType::Keyboard, key };
	}

	[[nodiscard]] constexpr InputControl MouseControl(MouseCode button) {
		return { InputDeviceType::Mouse, button };
	}

	struct RawInputEvent {
		InputControl Control;
		InputPhase Phase = InputPhase::Pressed;
		InputModifiers Modifiers = InputModifiers::None;
		glm::vec2 Value = {};
		char32_t Codepoint = 0;
		uint64_t Sequence = 0;
		double TimestampSeconds = 0.0;

		[[nodiscard]] static RawInputEvent Key(
			KeyCode key,
			InputPhase phase,
			InputModifiers modifiers = InputModifiers::None,
			double timestampSeconds = 0.0) {
			return { KeyboardControl(key), phase, modifiers, {}, 0, 0, timestampSeconds };
		}

		[[nodiscard]] static RawInputEvent MouseButton(
			MouseCode button,
			InputPhase phase,
			InputModifiers modifiers = InputModifiers::None,
			double timestampSeconds = 0.0) {
			return { MouseControl(button), phase, modifiers, {}, 0, 0, timestampSeconds };
		}

		[[nodiscard]] static RawInputEvent Pointer(glm::vec2 position, double timestampSeconds = 0.0) {
			return { MouseControl(Mouse::ButtonLast), InputPhase::Moved, InputModifiers::None, position, 0, 0, timestampSeconds };
		}

		[[nodiscard]] static RawInputEvent Scroll(glm::vec2 delta, double timestampSeconds = 0.0) {
			return { MouseControl(Mouse::ButtonLast), InputPhase::Scrolled, InputModifiers::None, delta, 0, 0, timestampSeconds };
		}

		[[nodiscard]] static RawInputEvent Text(char32_t codepoint, double timestampSeconds = 0.0) {
			RawInputEvent event;
			event.Phase = InputPhase::Text;
			event.Codepoint = codepoint;
			event.TimestampSeconds = timestampSeconds;
			return event;
		}
	};
}
