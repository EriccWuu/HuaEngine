#include "enginepch.h"
#include "ShaderDescriptor.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>

#include "yaml-cpp/yaml.h"

namespace {
	using namespace HE::Rendering;

	HE::ResultEnvelope Failure(const std::filesystem::path& path, std::string message) {
		return HE::ResultEnvelope::Failure("asset.shader_descriptor", path.generic_string(), std::move(message));
	}

	bool IsIdentifier(std::string_view value) {
		if (value.empty() || !(std::isalpha(static_cast<unsigned char>(value.front())) || value.front() == '_')) return false;
		return std::all_of(value.begin() + 1, value.end(), [](char character) {
			return std::isalnum(static_cast<unsigned char>(character)) || character == '_';
		});
	}

	bool IsSafeSourcePath(const std::filesystem::path& path) {
		if (path.empty() || path.is_absolute() || path.has_root_name()) return false;
		const auto normalized = path.lexically_normal();
		if (normalized != path || normalized.extension() != ".hlsl") return false;
		return std::none_of(normalized.begin(), normalized.end(), [](const auto& part) { return part == ".."; });
	}

	bool HasUniqueKeys(const YAML::Node& node) {
		if (!node.IsMap()) return false;
		std::set<std::string> keys;
		for (const auto& item : node) {
			if (!item.first.IsScalar() || !keys.emplace(item.first.as<std::string>()).second) return false;
		}
		return true;
	}

	bool ParseParameter(const std::string& name, const YAML::Node& node, ShaderParameterMetadata& output) {
		if (!IsIdentifier(name) || !HasUniqueKeys(node) || !node["scope"] || !node["editor"]) return false;
		output.Name = name;
		output.DisplayName = node["display_name"] ? node["display_name"].as<std::string>() : name;
		const auto scope = node["scope"].as<std::string>();
		if (scope == "Frame") output.Scope = ShaderParameterScope::Frame;
		else if (scope == "Material") output.Scope = ShaderParameterScope::Material;
		else if (scope == "Object") output.Scope = ShaderParameterScope::Object;
		else return false;

		const auto editor = node["editor"].as<std::string>();
		if (editor == "Color") {
			output.Editor = ShaderEditorKind::Color;
			output.Type = ShaderValueType::Float4;
			const auto value = node["default"];
			if (!value || !value.IsSequence() || value.size() != 4) return false;
			output.DefaultValue = glm::vec4(value[0].as<float>(), value[1].as<float>(), value[2].as<float>(), value[3].as<float>());
		}
		else if (editor == "Texture2D") {
			output.Editor = ShaderEditorKind::Texture2D;
			output.Type = ShaderValueType::Texture2D;
			output.DefaultValue = node["default"] && !node["default"].IsNull() ? node["default"].as<std::string>() : std::string();
		}
		else if (editor == "Default") {
			output.Editor = ShaderEditorKind::Default;
			const auto value = node["default"];
			if (!value) return false;
			if (value.IsScalar()) { output.Type = ShaderValueType::Float; output.DefaultValue = value.as<float>(); }
			else if (value.IsSequence() && value.size() == 2) { output.Type = ShaderValueType::Float2; output.DefaultValue = glm::vec2(value[0].as<float>(), value[1].as<float>()); }
			else if (value.IsSequence() && value.size() == 3) { output.Type = ShaderValueType::Float3; output.DefaultValue = glm::vec3(value[0].as<float>(), value[1].as<float>(), value[2].as<float>()); }
			else if (value.IsSequence() && value.size() == 4) { output.Type = ShaderValueType::Float4; output.DefaultValue = glm::vec4(value[0].as<float>(), value[1].as<float>(), value[2].as<float>(), value[3].as<float>()); }
			else return false;
		}
		else return false;
		if (node["range"]) {
			if (!node["range"].IsSequence() || node["range"].size() != 2) return false;
			output.Range = { node["range"][0].as<float>(), node["range"][1].as<float>() };
		}
		if (node["step"]) output.Step = node["step"].as<float>();
		if (node["tooltip"]) output.Tooltip = node["tooltip"].as<std::string>();
		return !output.DisplayName.empty();
	}

	YAML::Node EmitParameter(const ShaderParameterMetadata& parameter) {
		YAML::Node node;
		node["display_name"] = parameter.DisplayName;
		node["scope"] = parameter.Scope == ShaderParameterScope::Frame ? "Frame" : parameter.Scope == ShaderParameterScope::Object ? "Object" : "Material";
		if (parameter.Editor == ShaderEditorKind::Color) {
			node["editor"] = "Color";
			const auto value = std::get<glm::vec4>(parameter.DefaultValue);
			for (size_t index = 0; index < 4; ++index) node["default"].push_back(value[index]);
		}
		else if (parameter.Editor == ShaderEditorKind::Texture2D) {
			node["editor"] = "Texture2D";
			const auto& value = std::get<std::string>(parameter.DefaultValue);
			if (!value.empty()) node["default"] = value;
		}
		else {
			node["editor"] = "Default";
			std::visit([&](const auto& value) {
				using T = std::decay_t<decltype(value)>;
				if constexpr (std::is_same_v<T, float>) node["default"] = value;
				else if constexpr (std::is_same_v<T, glm::vec2> || std::is_same_v<T, glm::vec3> || std::is_same_v<T, glm::vec4>) for (glm::length_t index = 0; index < value.length(); ++index) node["default"].push_back(value[index]);
			}, parameter.DefaultValue);
		}
		if (!parameter.Range.empty()) for (const auto value : parameter.Range) node["range"].push_back(value);
		if (parameter.Step != 0.0f) node["step"] = parameter.Step;
		if (!parameter.Tooltip.empty()) node["tooltip"] = parameter.Tooltip;
		return node;
	}
}

namespace HE {
	ResultEnvelope LoadShaderDescriptor(const std::filesystem::path& path, ShaderDescriptor& outDescriptor) {
		outDescriptor = {};
		try {
			const auto root = YAML::LoadFile(path.string());
			if (!HasUniqueKeys(root) || !root["name"] || !root["language"] || !root["source"] || !root["stages"] || !root["parameters"]) return Failure(path, "Shader descriptor is missing required fields");
			if (root["language"].as<std::string>() != "HLSL") return Failure(path, "Shader language must be HLSL");
			outDescriptor.Name = root["name"].as<std::string>();
			outDescriptor.Source = std::filesystem::path(root["source"].as<std::string>());
			if (outDescriptor.Name.empty() || !IsSafeSourcePath(outDescriptor.Source)) return Failure(path, "Shader name or source path is invalid");

			const auto stages = root["stages"];
			if (!HasUniqueKeys(stages) || stages.size() != 2 || !stages["vertex"] || !stages["fragment"]) return Failure(path, "Shader must define vertex and fragment stages");
			const auto parseStage = [&](const YAML::Node& node, std::string_view profile, ShaderDescriptorStage& output) {
				if (!HasUniqueKeys(node) || node.size() != 2 || !node["entry"] || !node["profile"]) return false;
				output.Entry = node["entry"].as<std::string>();
				output.Profile = node["profile"].as<std::string>();
				return IsIdentifier(output.Entry) && output.Profile == profile;
			};
			if (!parseStage(stages["vertex"], "vs_6_0", outDescriptor.Vertex) || !parseStage(stages["fragment"], "ps_6_0", outDescriptor.Fragment)) return Failure(path, "Shader stage entry or profile is invalid");

			const auto parameters = root["parameters"];
			if (!HasUniqueKeys(parameters)) return Failure(path, "Shader parameters must be a mapping with unique names");
			for (const auto& item : parameters) {
				ShaderParameterMetadata parameter;
				if (!ParseParameter(item.first.as<std::string>(), item.second, parameter)) return Failure(path, "Shader parameter metadata is invalid");
				outDescriptor.Parameters.emplace_back(std::move(parameter));
			}
			std::sort(outDescriptor.Parameters.begin(), outDescriptor.Parameters.end(), [](const auto& a, const auto& b) { return a.Name < b.Name; });
			return ResultEnvelope::Success("asset.shader_descriptor", path.generic_string(), "Shader descriptor loaded");
		}
		catch (const YAML::Exception& error) { return Failure(path, error.what()); }
	}

	ResultEnvelope SaveShaderDescriptor(const std::filesystem::path& path, const ShaderDescriptor& descriptor) {
		if (descriptor.Name.empty() || !IsSafeSourcePath(descriptor.Source) || !IsIdentifier(descriptor.Vertex.Entry) || !IsIdentifier(descriptor.Fragment.Entry) || descriptor.Vertex.Profile != "vs_6_0" || descriptor.Fragment.Profile != "ps_6_0") return Failure(path, "Shader descriptor is invalid");
		YAML::Node root;
		root["name"] = descriptor.Name;
		root["language"] = "HLSL";
		root["source"] = descriptor.Source.generic_string();
		root["stages"]["vertex"]["entry"] = descriptor.Vertex.Entry;
		root["stages"]["vertex"]["profile"] = descriptor.Vertex.Profile;
		root["stages"]["fragment"]["entry"] = descriptor.Fragment.Entry;
		root["stages"]["fragment"]["profile"] = descriptor.Fragment.Profile;
		root["parameters"] = YAML::Node(YAML::NodeType::Map);
		auto parameters = descriptor.Parameters;
		std::sort(parameters.begin(), parameters.end(), [](const auto& a, const auto& b) { return a.Name < b.Name; });
		for (const auto& parameter : parameters) root["parameters"][parameter.Name] = EmitParameter(parameter);
		std::ofstream stream(path, std::ios::out | std::ios::trunc);
		if (!stream.good()) return Failure(path, "Failed to open shader descriptor for writing");
		stream << root;
		if (!stream.good()) return Failure(path, "Failed to write shader descriptor");
		return ResultEnvelope::Success("asset.shader_descriptor", path.generic_string(), "Shader descriptor saved");
	}
}
