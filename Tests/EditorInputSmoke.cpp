#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "HuaEngine/Core/KeyCodes.h"
#include "HuaEngine/Input/InputSystem.h"
#include "Input/EditorInputBindingStorage.h"
#include "Input/EditorInputService.h"

namespace {
	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[EditorInputSmoke] " << message << std::endl;
			std::exit(1);
		}
	}

	HE::InputSnapshot PressKey(HE::KeyCode key, HE::InputModifiers modifiers = HE::InputModifiers::None) {
		HE::InputSystem input;
		input.BeginFrame();
		input.Submit(HE::RawInputEvent::Key(key, HE::InputPhase::Pressed, modifiers));
		return input.FinalizeFrame();
	}
}

int main() {
	HE::Editor::EditorInputService service;
	int globalCount = 0;
	int sceneCount = 0;
	bool sceneEnabled = true;
	Require(service.Commands().Register({
		.Id = "editor.global.w",
		.DisplayName = "Global W",
		.Category = "Test",
		.CanExecute = [] { return true; },
		.Execute = [&] { ++globalCount; }
	}).Succeeded(), "Expected global command registration");
	Require(service.Commands().Register({
		.Id = "editor.scene.w",
		.DisplayName = "Scene W",
		.Category = "Test",
		.CanExecute = [&] { return sceneEnabled; },
		.Execute = [&] { ++sceneCount; }
	}).Succeeded(), "Expected scene command registration");

	const HE::InputGesture wPressed{ HE::KeyboardControl(HE::Key::W), HE::InputModifiers::None, HE::InputTrigger::Pressed, true };
	Require(service.Bindings().RegisterDefaultCommand({ "global.w", "editor.global.w", "Global", wPressed, 0, true }).Succeeded(), "Expected global W binding");
	Require(service.Bindings().RegisterDefaultCommand({ "scene.w", "editor.scene.w", "SceneViewport", wPressed, 0, true }).Succeeded(), "Expected scene W binding");
	service.Contexts().BeginFrame();
	service.Contexts().Activate("SceneViewport", 500, true, true, false);
	Require(service.Resolve(PressKey(HE::Key::W)).Succeeded(), "Expected scene W resolution");
	Require(sceneCount == 1 && globalCount == 0, "Expected focused scene context to consume W before Global");

	sceneEnabled = false;
	service.Contexts().BeginFrame();
	service.Contexts().Activate("SceneViewport", 500, true, true, false);
	Require(service.Resolve(PressKey(HE::Key::W)).Succeeded(), "Expected disabled scene command fallback");
	Require(sceneCount == 1 && globalCount == 1, "Expected disabled scene command to fall back to Global");

	int conflictCount = 0;
	Require(service.Commands().Register({ "editor.scene.conflict", "Conflict", "Test", [] { return true; }, [&] { ++conflictCount; } }).Succeeded(), "Expected conflict command registration");
	Require(service.Bindings().RegisterDefaultCommand({ "scene.conflict", "editor.scene.conflict", "SceneViewport", wPressed, 0, true }).Succeeded(), "Expected conflicting binding registration");
	sceneEnabled = true;
	service.Contexts().BeginFrame();
	service.Contexts().Activate("SceneViewport", 500, true, true, false);
	const auto conflictResult = service.Resolve(PressKey(HE::Key::W));
	Require(conflictResult.Failed() && sceneCount == 1 && conflictCount == 0, "Expected hard conflict to execute neither command");

	HE::Editor::EditorInputService actionService;
	const HE::InputGesture heldW{ HE::KeyboardControl(HE::Key::W), HE::InputModifiers::None, HE::InputTrigger::Held, true };
	Require(actionService.Bindings().RegisterDefaultAction({ "camera.forward", "editor.camera.forward", "SceneViewport", heldW, 1.0f }).Succeeded(), "Expected camera action binding");
	actionService.Contexts().BeginFrame();
	actionService.Contexts().Activate("SceneViewport", 500, true, true, false);
	HE::InputSystem heldInput;
	heldInput.BeginFrame();
	heldInput.Submit(HE::RawInputEvent::Key(HE::Key::W, HE::InputPhase::Pressed));
	const auto& heldSnapshot = heldInput.FinalizeFrame();
	Require(actionService.Resolve(heldSnapshot).Succeeded(), "Expected held action resolution");
	Require(actionService.GetActionValue("editor.camera.forward") == 1.0f, "Expected held W action value");

	const auto storageRoot = std::filesystem::temp_directory_path() / "HuaEngineEditorInputSmoke";
	std::error_code errorCode;
	std::filesystem::remove_all(storageRoot, errorCode);
	const auto storagePath = storageRoot / "input-bindings.json";
	const HE::Editor::EditorInputBindingOverride overrideBinding{
		.CommandId = "editor.global.w",
		.ContextId = "Global",
		.Gesture = { HE::KeyboardControl(HE::Key::S), HE::InputModifiers::Control, HE::InputTrigger::Pressed, true }
	};
	Require(HE::Editor::EditorInputBindingStorage::Save(storagePath, { overrideBinding }).Succeeded(), "Expected binding override save");
	std::vector<HE::Editor::EditorInputBindingOverride> loadedOverrides;
	Require(HE::Editor::EditorInputBindingStorage::Load(storagePath, loadedOverrides).Succeeded(), "Expected binding override load");
	Require(loadedOverrides.size() == 1 && loadedOverrides.front() == overrideBinding, "Expected binding override round trip");
	std::filesystem::remove_all(storageRoot, errorCode);

	std::cout << "EditorInputSmoke passed" << std::endl;
	return 0;
}
