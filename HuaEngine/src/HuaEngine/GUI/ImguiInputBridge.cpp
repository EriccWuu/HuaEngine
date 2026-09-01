#include "enginepch.h"
#include "HuaEngine/GUI/ImguiInputBridge.h"

#include "HuaEngine/Input/InputSystem.h"
#include "imgui.h"

namespace HE {
	ImGuiKey KeyToImGuiKey(int keycode);

	namespace {
		InputModifiers GetImguiModifiers(const ImGuiIO& io) {
			InputModifiers modifiers = InputModifiers::None;
			if (io.KeyShift) modifiers = modifiers | InputModifiers::Shift;
			if (io.KeyCtrl) modifiers = modifiers | InputModifiers::Control;
			if (io.KeyAlt) modifiers = modifiers | InputModifiers::Alt;
			if (io.KeySuper) modifiers = modifiers | InputModifiers::Super;
			return modifiers;
		}
	}

	void SynchronizeImguiInput(InputSystem& inputSystem) {
		const ImGuiIO& io = ImGui::GetIO();
		inputSystem.SetCaptureState({ io.WantCaptureKeyboard, io.WantCaptureMouse, io.WantTextInput });
		const InputModifiers modifiers = GetImguiModifiers(io);
		for (int keyCode = 0; keyCode <= static_cast<int>(Key::Menu); ++keyCode) {
			const ImGuiKey imguiKey = KeyToImGuiKey(keyCode);
			if (imguiKey == ImGuiKey_None) continue;
			const InputControl control = KeyboardControl(static_cast<KeyCode>(keyCode));
			const bool down = ImGui::IsKeyDown(imguiKey);
			if (down != inputSystem.IsWorkingDown(control)) {
				inputSystem.Submit(RawInputEvent::Key(
					static_cast<KeyCode>(keyCode),
					down ? InputPhase::Pressed : InputPhase::Released,
					modifiers,
					ImGui::GetTime()));
			}
		}

		for (int button = 0; button <= static_cast<int>(Mouse::ButtonLast); ++button) {
			const InputControl control = MouseControl(static_cast<MouseCode>(button));
			const bool down = ImGui::IsMouseDown(button);
			if (down != inputSystem.IsWorkingDown(control)) {
				inputSystem.Submit(RawInputEvent::MouseButton(
					static_cast<MouseCode>(button),
					down ? InputPhase::Pressed : InputPhase::Released,
					modifiers,
					ImGui::GetTime()));
			}
		}

		if (io.MousePos.x > -FLT_MAX && io.MousePos.y > -FLT_MAX) {
			inputSystem.Submit(RawInputEvent::Pointer({ io.MousePos.x, io.MousePos.y }, ImGui::GetTime()));
		}
		const glm::vec2 bridgeScroll = { io.MouseWheelH, io.MouseWheel };
		inputSystem.Submit(RawInputEvent::Scroll(bridgeScroll - inputSystem.GetWorkingScrollDelta(), ImGui::GetTime()));
	}
}
