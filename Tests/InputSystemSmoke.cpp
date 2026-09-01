#include <cstdlib>
#include <iostream>
#include <string>

#include "HuaEngine/Core/KeyCodes.h"
#include "HuaEngine/Core/MouseCodes.h"
#include "HuaEngine/ECS/FrameContext.h"
#include "HuaEngine/Input/InputSystem.h"

namespace {
	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[InputSystemSmoke] " << message << std::endl;
			std::exit(1);
		}
	}
}

int main() {
	HE::InputSystem input;
	const auto w = HE::KeyboardControl(HE::Key::W);
	const auto leftMouse = HE::MouseControl(HE::Mouse::ButtonLeft);

	input.BeginFrame();
	input.Submit(HE::RawInputEvent::Key(HE::Key::W, HE::InputPhase::Pressed));
	const auto& pressed = input.FinalizeFrame();
	Require(pressed.WasPressed(w) && pressed.IsDown(w), "Expected W press edge and held state");
	HE::FrameContext frame;
	frame.GetOrCreate<HE::InputFrameState>().Snapshot = &pressed;
	Require(frame.TryGet<HE::InputFrameState>()->Snapshot == &pressed, "Expected InputSnapshot publication through FrameContext");

	input.BeginFrame();
	const auto& held = input.FinalizeFrame();
	Require(!held.WasPressed(w) && held.IsDown(w), "Expected held W without a second press edge");

	input.BeginFrame();
	input.Submit(HE::RawInputEvent::Key(HE::Key::W, HE::InputPhase::Repeated));
	const auto& repeated = input.FinalizeFrame();
	Require(repeated.WasRepeated(w) && !repeated.WasPressed(w), "Expected repeat separate from press");

	input.BeginFrame();
	input.Submit(HE::RawInputEvent::Pointer({ 10.0f, 20.0f }));
	input.Submit(HE::RawInputEvent::Pointer({ 14.0f, 27.0f }));
	input.Submit(HE::RawInputEvent::Scroll({ 1.0f, -2.0f }));
	const auto& pointer = input.FinalizeFrame();
	Require(pointer.GetPointerPosition() == glm::vec2(14.0f, 27.0f), "Expected latest pointer position");
	Require(pointer.GetPointerDelta() == glm::vec2(14.0f, 27.0f), "Expected accumulated pointer delta");
	Require(pointer.GetScrollDelta() == glm::vec2(1.0f, -2.0f), "Expected accumulated scroll delta");

	input.BeginFrame();
	const auto& clearedDelta = input.FinalizeFrame();
	Require(clearedDelta.GetPointerDelta() == glm::vec2(0.0f) && clearedDelta.GetScrollDelta() == glm::vec2(0.0f), "Expected transient pointer values cleared each frame");

	input.BeginFrame();
	input.Submit(HE::RawInputEvent::Pointer({ 20.0f, 30.0f }, 1.0));
	input.Submit(HE::RawInputEvent::MouseButton(HE::Mouse::ButtonLeft, HE::InputPhase::Pressed, HE::InputModifiers::None, 1.0));
	input.Submit(HE::RawInputEvent::MouseButton(HE::Mouse::ButtonLeft, HE::InputPhase::Released, HE::InputModifiers::None, 1.05));
	input.Submit(HE::RawInputEvent::MouseButton(HE::Mouse::ButtonLeft, HE::InputPhase::Pressed, HE::InputModifiers::None, 1.20));
	const auto& doublePressed = input.FinalizeFrame();
	Require(doublePressed.WasDoublePressed(leftMouse), "Expected nearby mouse presses to produce a double-press edge");

	input.BeginFrame();
	input.HandleFocusLost();
	const auto& focusLost = input.FinalizeFrame();
	Require(focusLost.WasReleased(w) && !focusLost.IsDown(w), "Expected focus loss to release held controls");
	Require(!focusLost.GetEvents().empty(), "Expected ordered focus-loss release events");

	std::cout << "InputSystemSmoke passed" << std::endl;
	return 0;
}
