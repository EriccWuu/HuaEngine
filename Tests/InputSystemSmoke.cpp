#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "HuaEngine/Core/KeyCodes.h"
#include "HuaEngine/Core/MouseCodes.h"
#include "HuaEngine/ECS/FrameContext.h"
#include "HuaEngine/Input/InputSystem.h"
#include "Platform/Windows/GlfwInputTranslator.h"
#include "GLFW/glfw3.h"

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

	const auto translated = HE::TranslateGlfwKeyEvent(
		GLFW_KEY_S,
		GLFW_PRESS,
		GLFW_MOD_CONTROL | GLFW_MOD_SHIFT,
		2.0);
	Require(translated.Control == HE::KeyboardControl(HE::Key::S), "Expected platform key translation");
	Require(
		translated.Phase == HE::InputPhase::Pressed &&
			HE::HasModifier(translated.Modifiers, HE::InputModifiers::Control) &&
			HE::HasModifier(translated.Modifiers, HE::InputModifiers::Shift),
		"Expected platform phase and modifier translation");

	std::filesystem::path repositoryRoot = std::filesystem::current_path();
	while (!repositoryRoot.empty() && !std::filesystem::exists(repositoryRoot / "CMakeLists.txt")) {
		repositoryRoot = repositoryRoot.parent_path();
	}
	Require(!repositoryRoot.empty(), "Expected to locate repository root");
	std::ifstream applicationStream(repositoryRoot / "HuaEngine" / "src" / "HuaEngine" / "Application.cpp");
	Require(applicationStream.good(), "Expected Application.cpp to be readable");
	std::stringstream applicationBuffer;
	applicationBuffer << applicationStream.rdbuf();
	const std::string applicationSource = applicationBuffer.str();
	const size_t pollPosition = applicationSource.find("m_Window->PollEvents()");
	const size_t updatePosition = applicationSource.find("layer->OnUpdate()");
	const size_t presentPosition = applicationSource.find("m_Window->Present()");
	Require(
		pollPosition != std::string::npos && updatePosition != std::string::npos && presentPosition != std::string::npos &&
			pollPosition < updatePosition && updatePosition < presentPosition,
		"Expected input polling before layer update and presentation after update");

	std::cout << "InputSystemSmoke passed" << std::endl;
	return 0;
}
