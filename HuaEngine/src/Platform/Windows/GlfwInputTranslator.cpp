#include "enginepch.h"
#include "Platform/Windows/GlfwInputTranslator.h"

#include <GLFW/glfw3.h>

namespace HE {
	InputModifiers TranslateGlfwModifiers(int modifiers) {
		InputModifiers result = InputModifiers::None;
		if ((modifiers & GLFW_MOD_SHIFT) != 0) result = result | InputModifiers::Shift;
		if ((modifiers & GLFW_MOD_CONTROL) != 0) result = result | InputModifiers::Control;
		if ((modifiers & GLFW_MOD_ALT) != 0) result = result | InputModifiers::Alt;
		if ((modifiers & GLFW_MOD_SUPER) != 0) result = result | InputModifiers::Super;
		return result;
	}

	RawInputEvent TranslateGlfwKeyEvent(int key, int action, int modifiers, double timestampSeconds) {
		InputPhase phase = InputPhase::Repeated;
		if (action == GLFW_PRESS) phase = InputPhase::Pressed;
		else if (action == GLFW_RELEASE) phase = InputPhase::Released;
		return RawInputEvent::Key(static_cast<KeyCode>(key), phase, TranslateGlfwModifiers(modifiers), timestampSeconds);
	}

	RawInputEvent TranslateGlfwMouseButtonEvent(int button, int action, int modifiers, double timestampSeconds) {
		const InputPhase phase = action == GLFW_RELEASE ? InputPhase::Released : InputPhase::Pressed;
		return RawInputEvent::MouseButton(static_cast<MouseCode>(button), phase, TranslateGlfwModifiers(modifiers), timestampSeconds);
	}
}
