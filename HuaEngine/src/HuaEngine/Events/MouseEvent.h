#pragma once
#include "Event.h"
#include "HuaEngine/Core/MouseCodes.h"

namespace HE {
	class MouseMovedEvent : public Event {
	public:
		MouseMovedEvent(const float x, const float y): m_x(x), m_y(y) {}

		float GetX() { return m_x; }
		float GetY() { return m_y; }

		std::string ToString() const override {
			std::stringstream ss;
			ss << "MouseMovedEvent£º Position (" << m_x << ", " << m_y << ")";
			return ss.str();
		}

		EVENT_CLASS_TYPE(MouseMoved)
		EVENT_CLASS_CATEGORY(EventCategory::EventCategoryMouse | EventCategory::EventCategoryInput)

	private:
		float m_x, m_y;
	};

	class MouseScrolledEvent : public Event {
	public:
		MouseScrolledEvent(const float xoffset, const float yoffset) : m_xoffset(xoffset), m_yoffset(yoffset) {}

		float GetXOffset() { return m_xoffset; }
		float GetYOffset() { return m_yoffset; }

		std::string ToString() const override {
			std::stringstream ss;
			ss << "MouseScrolledEvent£º Offset (" << m_xoffset << ", " << m_yoffset << ")";
			return ss.str();
		}

		EVENT_CLASS_TYPE(MouseScrolled)
		EVENT_CLASS_CATEGORY(EventCategory::EventCategoryMouse | EventCategory::EventCategoryInput)

	private:
		float m_xoffset, m_yoffset;
	};

	class MouseButtonEvent : public Event {
	public:
		MouseCode GetMouseButton() { return m_Code; }

		EVENT_CLASS_CATEGORY(EventCategory::EventCategoryMouse | EventCategory::EventCategoryInput)

	protected:
		MouseButtonEvent(MouseCode code) : m_Code(code) {}

		MouseCode m_Code;
	};

	class MouseButtonPressedEvent : public MouseButtonEvent {
	public:
		MouseButtonPressedEvent(MouseCode code) : MouseButtonEvent(code) {}

		std::string ToString() const override {
			std::stringstream ss;
			ss << "MouseButtonPressedEvent£º MouseCode " << m_Code;
			return ss.str();
		}

		EVENT_CLASS_TYPE(MouseButtonPressed)
	};

	class MouseButtonReleasedEvent : public MouseButtonEvent {
	public:
		MouseButtonReleasedEvent(MouseCode code) : MouseButtonEvent(code) {}

		std::string ToString() const override {
			std::stringstream ss;
			ss << "MouseButtonReleasedEvent£º MouseCode " << m_Code;
			return ss.str();
		}

		EVENT_CLASS_TYPE(MouseButtonReleased)
	};
}