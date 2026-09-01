#pragma once

#include <span>
#include <unordered_set>
#include <vector>

#include "HuaEngine/Input/InputTypes.h"

namespace HE {
	class InputSystem;

	class InputSnapshot {
	public:
		[[nodiscard]] bool IsDown(InputControl control) const;
		[[nodiscard]] bool WasPressed(InputControl control) const;
		[[nodiscard]] bool WasReleased(InputControl control) const;
		[[nodiscard]] bool WasRepeated(InputControl control) const;
		[[nodiscard]] bool WasDoublePressed(InputControl control) const;

		[[nodiscard]] glm::vec2 GetPointerPosition() const { return m_PointerPosition; }
		[[nodiscard]] glm::vec2 GetPointerDelta() const { return m_PointerDelta; }
		[[nodiscard]] glm::vec2 GetScrollDelta() const { return m_ScrollDelta; }
		[[nodiscard]] InputModifiers GetModifiers() const { return m_Modifiers; }
		[[nodiscard]] std::span<const RawInputEvent> GetEvents() const { return m_Events; }

	private:
		friend class InputSystem;

		std::unordered_set<InputControl, InputControlHash> m_Down;
		std::unordered_set<InputControl, InputControlHash> m_Pressed;
		std::unordered_set<InputControl, InputControlHash> m_Released;
		std::unordered_set<InputControl, InputControlHash> m_Repeated;
		std::unordered_set<InputControl, InputControlHash> m_DoublePressed;
		std::vector<RawInputEvent> m_Events;
		glm::vec2 m_PointerPosition = {};
		glm::vec2 m_PointerDelta = {};
		glm::vec2 m_ScrollDelta = {};
		InputModifiers m_Modifiers = InputModifiers::None;
	};

	struct InputFrameState {
		const InputSnapshot* Snapshot = nullptr;
	};
}
