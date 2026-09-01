#include "enginepch.h"
#include "HuaEngine/Input/InputSystem.h"

namespace HE {
	namespace {
		constexpr double DoublePressSeconds = 0.35;
		constexpr float DoublePressDistanceSquared = 16.0f;
	}

	void InputSystem::BeginFrame() {
		m_Pressed.clear();
		m_Released.clear();
		m_Repeated.clear();
		m_DoublePressed.clear();
		m_Events.clear();
		m_PointerDelta = {};
		m_ScrollDelta = {};
	}

	void InputSystem::Submit(RawInputEvent event) {
		if (event.Sequence == 0) {
			event.Sequence = m_NextSequence++;
		} else {
			m_NextSequence = (std::max)(m_NextSequence, event.Sequence + 1);
		}

		m_Modifiers = event.Modifiers;
		bool accepted = true;
		switch (event.Phase) {
		case InputPhase::Pressed:
		case InputPhase::Released:
		case InputPhase::Repeated:
			accepted = RecordControlEvent(event);
			break;
		case InputPhase::Moved:
			if (event.Value == m_PointerPosition) {
				accepted = false;
				break;
			}
			m_PointerDelta += event.Value - m_PointerPosition;
			m_PointerPosition = event.Value;
			break;
		case InputPhase::Scrolled:
			if (event.Value == glm::vec2(0.0f)) {
				accepted = false;
				break;
			}
			m_ScrollDelta += event.Value;
			break;
		case InputPhase::Text:
			break;
		}

		if (accepted) {
			m_Events.push_back(std::move(event));
		}
	}

	bool InputSystem::RecordControlEvent(const RawInputEvent& event) {
		switch (event.Phase) {
		case InputPhase::Pressed:
			if (m_Down.contains(event.Control)) return false;
			m_Pressed.insert(event.Control);
			m_Down.insert(event.Control);
			DetectDoublePress(event);
			return true;
		case InputPhase::Released:
			if (m_Down.erase(event.Control) == 0) return false;
			m_Released.insert(event.Control);
			return true;
		case InputPhase::Repeated:
			m_Down.insert(event.Control);
			m_Repeated.insert(event.Control);
			return true;
		default:
			return false;
		}
	}

	void InputSystem::DetectDoublePress(const RawInputEvent& event) {
		if (event.Control.Device != InputDeviceType::Mouse || event.TimestampSeconds <= 0.0) {
			return;
		}

		auto& previous = m_LastPress[event.Control];
		const glm::vec2 delta = m_PointerPosition - previous.PointerPosition;
		const float distanceSquared = delta.x * delta.x + delta.y * delta.y;
		if (previous.Valid &&
			event.TimestampSeconds >= previous.TimestampSeconds &&
			event.TimestampSeconds - previous.TimestampSeconds <= DoublePressSeconds &&
			distanceSquared <= DoublePressDistanceSquared) {
			m_DoublePressed.insert(event.Control);
			previous.Valid = false;
			return;
		}

		previous = { event.TimestampSeconds, m_PointerPosition, true };
	}

	void InputSystem::HandleFocusLost() {
		std::vector<InputControl> held(m_Down.begin(), m_Down.end());
		for (const InputControl control : held) {
			RawInputEvent release;
			release.Control = control;
			release.Phase = InputPhase::Released;
			release.Modifiers = InputModifiers::None;
			Submit(std::move(release));
		}
		m_Modifiers = InputModifiers::None;
	}

	const InputSnapshot& InputSystem::FinalizeFrame() {
		m_Snapshot.m_Down = m_Down;
		m_Snapshot.m_Pressed = m_Pressed;
		m_Snapshot.m_Released = m_Released;
		m_Snapshot.m_Repeated = m_Repeated;
		m_Snapshot.m_DoublePressed = m_DoublePressed;
		m_Snapshot.m_Events = m_Events;
		m_Snapshot.m_PointerPosition = m_PointerPosition;
		m_Snapshot.m_PointerDelta = m_PointerDelta;
		m_Snapshot.m_ScrollDelta = m_ScrollDelta;
		m_Snapshot.m_Modifiers = m_Modifiers;
		return m_Snapshot;
	}
}
