#pragma once

#include "Core.h"
#include "KeyCodes.h"
#include "MouseCodes.h"

namespace HE {
	class ENGINE_API Input {
	public:
		inline static bool IsKeyPressed(KeyCode keycode) { return m_Instance->IsKeyPressedImpl(keycode); }
		inline static bool IsMousePressed(MouseCode mousecode) { return m_Instance->IsMousePressedImpl(mousecode); }
		inline static float GetMouseX() { return m_Instance->GetMouseXImpl(); }
		inline static float GetMouseY() { return m_Instance->GetMouseYImpl(); }

	protected:
		virtual bool IsKeyPressedImpl(KeyCode keycode) = 0;
		virtual bool IsMousePressedImpl(MouseCode mousecode) = 0;
		virtual float GetMouseXImpl() = 0;
		virtual float GetMouseYImpl() = 0;

	private:
		static Input* m_Instance;
	};
}