#pragma once

#include <unordered_map>

#include "HuaEngine/Input/InputSnapshot.h"

namespace HE {
	class InputSystem {
	public:
		void BeginFrame();
		void Submit(RawInputEvent event);
		void HandleFocusLost();
		[[nodiscard]] const InputSnapshot& FinalizeFrame();
		[[nodiscard]] const InputSnapshot& GetSnapshot() const { return m_Snapshot; }
		[[nodiscard]] bool IsWorkingDown(InputControl control) const { return m_Down.contains(control); }
		[[nodiscard]] glm::vec2 GetWorkingPointerPosition() const { return m_PointerPosition; }
		[[nodiscard]] glm::vec2 GetWorkingScrollDelta() const { return m_ScrollDelta; }

	private:
		struct PressRecord {
			double TimestampSeconds = 0.0;
			glm::vec2 PointerPosition = {};
			bool Valid = false;
		};

		[[nodiscard]] bool RecordControlEvent(const RawInputEvent& event);
		void DetectDoublePress(const RawInputEvent& event);

		InputSnapshot m_Snapshot;
		std::unordered_set<InputControl, InputControlHash> m_Down;
		std::unordered_set<InputControl, InputControlHash> m_Pressed;
		std::unordered_set<InputControl, InputControlHash> m_Released;
		std::unordered_set<InputControl, InputControlHash> m_Repeated;
		std::unordered_set<InputControl, InputControlHash> m_DoublePressed;
		std::unordered_map<InputControl, PressRecord, InputControlHash> m_LastPress;
		std::vector<RawInputEvent> m_Events;
		glm::vec2 m_PointerPosition = {};
		glm::vec2 m_PointerDelta = {};
		glm::vec2 m_ScrollDelta = {};
		InputModifiers m_Modifiers = InputModifiers::None;
		uint64_t m_NextSequence = 1;
	};
}
