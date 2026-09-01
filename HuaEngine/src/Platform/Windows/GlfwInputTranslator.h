#pragma once

#include "HuaEngine/Input/InputTypes.h"

namespace HE {
	[[nodiscard]] InputModifiers TranslateGlfwModifiers(int modifiers);
	[[nodiscard]] RawInputEvent TranslateGlfwKeyEvent(int key, int action, int modifiers, double timestampSeconds);
	[[nodiscard]] RawInputEvent TranslateGlfwMouseButtonEvent(int button, int action, int modifiers, double timestampSeconds);
}
