#include "EditorInputBindingStorage.h"

#include <cstdlib>
#include <fstream>

#include <yaml-cpp/yaml.h>

namespace HE::Editor {
	std::filesystem::path EditorInputBindingStorage::GetDefaultPath() {
		if (const char* localAppData = std::getenv("LOCALAPPDATA")) {
			return std::filesystem::path(localAppData) / "HuaEngine" / "Editor" / "input-bindings.json";
		}
		return std::filesystem::current_path() / ".workspace" / "input-bindings.json";
	}

	ResultEnvelope EditorInputBindingStorage::Save(const std::filesystem::path& path, const std::vector<EditorInputBindingOverride>& overrides) {
		std::error_code errorCode;
		std::filesystem::create_directories(path.parent_path(), errorCode);
		if (errorCode) return ResultEnvelope::Failure("editor.input.bindings.save", path.string(), "Binding directory could not be created");
		std::ofstream output(path, std::ios::binary | std::ios::trunc);
		if (!output) return ResultEnvelope::Failure("editor.input.bindings.save", path.string(), "Binding file could not be opened");
		output << "{\n  \"version\": 1,\n  \"overrides\": [";
		for (size_t index = 0; index < overrides.size(); ++index) {
			const auto& binding = overrides[index];
			output << (index == 0 ? "\n" : ",\n")
				<< "    { \"command_id\": \"" << binding.CommandId
				<< "\", \"context_id\": \"" << binding.ContextId
				<< "\", \"device\": " << static_cast<int>(binding.Gesture.Primary.Device)
				<< ", \"code\": " << binding.Gesture.Primary.Code
				<< ", \"modifiers\": " << static_cast<int>(binding.Gesture.Modifiers)
				<< ", \"trigger\": " << static_cast<int>(binding.Gesture.Trigger)
				<< ", \"exact_modifiers\": " << (binding.Gesture.ExactModifiers ? "true" : "false") << " }";
		}
		output << (overrides.empty() ? "" : "\n  ") << "]\n}\n";
		if (!output.good()) return ResultEnvelope::Failure("editor.input.bindings.save", path.string(), "Binding file could not be written");
		return ResultEnvelope::Success("editor.input.bindings.save", path.string(), "Binding overrides saved");
	}

	ResultEnvelope EditorInputBindingStorage::Load(const std::filesystem::path& path, std::vector<EditorInputBindingOverride>& overrides) {
		overrides.clear();
		if (!std::filesystem::exists(path)) return ResultEnvelope::Success("editor.input.bindings.load", path.string(), "No user binding overrides exist");
		try {
			const auto root = YAML::LoadFile(path.string());
			if (!root["version"] || root["version"].as<int>() != 1 || !root["overrides"].IsSequence()) {
				return ResultEnvelope::Failure("editor.input.bindings.load", path.string(), "Binding file schema is invalid");
			}
			for (const auto& node : root["overrides"]) {
				EditorInputBindingOverride binding;
				binding.CommandId = node["command_id"].as<std::string>();
				binding.ContextId = node["context_id"].as<std::string>();
				binding.Gesture.Primary.Device = static_cast<InputDeviceType>(node["device"].as<int>());
				binding.Gesture.Primary.Code = node["code"].as<uint16_t>();
				binding.Gesture.Modifiers = static_cast<InputModifiers>(node["modifiers"].as<int>());
				binding.Gesture.Trigger = static_cast<InputTrigger>(node["trigger"].as<int>());
				binding.Gesture.ExactModifiers = node["exact_modifiers"].as<bool>();
				if (binding.CommandId.empty() || binding.ContextId.empty()) throw YAML::BadConversion(node.Mark());
				overrides.emplace_back(std::move(binding));
			}
		} catch (const YAML::Exception&) {
			overrides.clear();
			return ResultEnvelope::Failure("editor.input.bindings.load", path.string(), "Binding file could not be parsed");
		}
		return ResultEnvelope::Success("editor.input.bindings.load", path.string(), "Binding overrides loaded");
	}
}
