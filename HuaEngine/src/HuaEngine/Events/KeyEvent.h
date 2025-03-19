#pragma once
#include "HuaEngine/Events/Event.h"
#include "HuaEngine/Core/KeyCodes.h"

namespace HE {

	class KeyEvent : public Event {
	public:
		inline int GetKeyCode() {
			return m_KeyCode;
		}

		EVENT_CLASS_CATEGORY(EventCategoryKeyboard | EventCategoryInput)

	protected:
		KeyEvent(KeyCode keycode) : m_KeyCode(keycode) {}
		KeyCode m_KeyCode;
	};

	class KeyPressedEvent : public KeyEvent {
	public:
		KeyPressedEvent(KeyCode keycode, bool isRepeated) : KeyEvent(keycode), m_IsRepeaded(isRepeated) {}
		bool IsRepeated() { return m_IsRepeaded; }
		std::string ToString() const override {
			std::stringstream ss;
			ss << "KeyPressedEvent: keycode " << m_KeyCode << " (Repeated = " << m_IsRepeaded << ")";
			return ss.str();
		}

		EVENT_CLASS_TYPE(KeyPressed)
	private:
		bool m_IsRepeaded = false;
	};

	class KeyReleasedEvent : public KeyEvent {
	public:
		KeyReleasedEvent(KeyCode keycode) : KeyEvent(keycode) {}
		std::string ToString() const override {
			std::stringstream ss;
			ss << "KeyReleasedEvent: keycode " << m_KeyCode;
			return ss.str();
		}

		EVENT_CLASS_TYPE(KeyReleased)
	};

	class KeyTypedEvent : public KeyEvent {
	public:
		KeyTypedEvent(KeyCode keycode): KeyEvent(keycode) {}
		std::string ToString() const override {
			std::stringstream ss;
			ss << "KeyTypedEvent: keycode " << m_KeyCode;
			return ss.str();
		}
		EVENT_CLASS_TYPE(KeyTyped)
	};
}