#pragma once
#include "HuaEngine/Core/Input.h"

namespace HE {
	class WindowsInput : public Input {
	protected:
		virtual bool IsKeyPressedImpl(KeyCode keycode) override;
		virtual bool IsMousePressedImpl(MouseCode keycode) override;
		virtual float GetMouseXImpl() override;
		virtual float GetMouseYImpl() override;
	};
}