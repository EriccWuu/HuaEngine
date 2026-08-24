#include "enginepch.h"
#include "MaterialSourceData.h"

#include <limits>

#include "yaml-cpp/yaml.h"

namespace {
	using namespace HE::Rendering;

	bool ParseMaterialType(const std::string& value, MaterialType& outType) {
		if (value == "Standard") outType = MaterialType::Standard;
		else if (value == "Unlit") outType = MaterialType::Unlit;
		else if (value == "Custom") outType = MaterialType::Custom;
		else return false;
		return true;
	}

	bool ParseParameterType(const std::string& value, MaterialParameterType& outType) {
		if (value == "Int") outType = MaterialParameterType::Int;
		else if (value == "Float") outType = MaterialParameterType::Float;
		else if (value == "Vec2") outType = MaterialParameterType::Vec2;
		else if (value == "Vec3") outType = MaterialParameterType::Vec3;
		else if (value == "Vec4") outType = MaterialParameterType::Vec4;
		else if (value == "Mat3") outType = MaterialParameterType::Mat3;
		else if (value == "Mat4") outType = MaterialParameterType::Mat4;
		else if (value == "Texture2D") outType = MaterialParameterType::Texture2D;
		else if (value == "IntArray") outType = MaterialParameterType::IntArray;
		else if (value == "FloatArray") outType = MaterialParameterType::FloatArray;
		else return false;
		return true;
	}

	template<glm::length_t Length>
	bool ParseVector(const YAML::Node& node, glm::vec<Length, float>& outValue) {
		static constexpr const char* Names[] = { "x", "y", "z", "w" };
		if (!node.IsMap()) return false;
		for (glm::length_t index = 0; index < Length; ++index) {
			if (!node[Names[index]]) return false;
			outValue[index] = node[Names[index]].as<float>();
		}
		return true;
	}

	template<glm::length_t Length>
	bool ParseMatrix(const YAML::Node& node, glm::mat<Length, Length, float>& outValue) {
		if (!node.IsSequence() || node.size() != Length * Length) return false;
		for (glm::length_t column = 0; column < Length; ++column) {
			for (glm::length_t row = 0; row < Length; ++row) {
				outValue[column][row] = node[column * Length + row].as<float>();
			}
		}
		return true;
	}

	bool ParseParameterValue(
		const YAML::Node& node,
		MaterialParameterType type,
		MaterialSourceParameterValue& outValue) {
		switch (type) {
		case MaterialParameterType::Int:
			outValue = node.as<int>();
			return true;
		case MaterialParameterType::Float:
			outValue = node.as<float>();
			return true;
		case MaterialParameterType::Vec2: {
			glm::vec2 value{};
			if (!ParseVector(node, value)) return false;
			outValue = value;
			return true;
		}
		case MaterialParameterType::Vec3: {
			glm::vec3 value{};
			if (!ParseVector(node, value)) return false;
			outValue = value;
			return true;
		}
		case MaterialParameterType::Vec4: {
			glm::vec4 value{};
			if (!ParseVector(node, value)) return false;
			outValue = value;
			return true;
		}
		case MaterialParameterType::Mat3: {
			glm::mat3 value{ 1.0f };
			if (!ParseMatrix(node, value)) return false;
			outValue = value;
			return true;
		}
		case MaterialParameterType::Mat4: {
			glm::mat4 value{ 1.0f };
			if (!ParseMatrix(node, value)) return false;
			outValue = value;
			return true;
		}
		case MaterialParameterType::Texture2D:
			outValue = node.IsNull() ? std::string() : node.as<std::string>();
			return true;
		case MaterialParameterType::IntArray: {
			if (!node.IsSequence()) return false;
			std::vector<int> values;
			values.reserve(node.size());
			for (const auto& item : node) values.push_back(item.as<int>());
			outValue = std::move(values);
			return true;
		}
		case MaterialParameterType::FloatArray: {
			if (!node.IsSequence()) return false;
			std::vector<float> values;
			values.reserve(node.size());
			for (const auto& item : node) values.push_back(item.as<float>());
			outValue = std::move(values);
			return true;
		}
		case MaterialParameterType::TextureCube:
			return false;
		}
		return false;
	}

	bool ParseUntypedParameterValue(const YAML::Node& node, MaterialSourceParameter& parameter) {
		if (node.IsScalar()) {
			try { parameter.Value = node.as<float>(); parameter.Type = MaterialParameterType::Float; return true; }
			catch (const YAML::Exception&) { parameter.Value = node.as<std::string>(); parameter.Type = MaterialParameterType::Texture2D; return true; }
		}
		if (node.IsMap()) {
			if (node["w"]) { glm::vec4 value{}; if (!ParseVector(node, value)) return false; parameter.Type = MaterialParameterType::Vec4; parameter.Value = value; return true; }
			if (node["z"]) { glm::vec3 value{}; if (!ParseVector(node, value)) return false; parameter.Type = MaterialParameterType::Vec3; parameter.Value = value; return true; }
			if (node["y"]) { glm::vec2 value{}; if (!ParseVector(node, value)) return false; parameter.Type = MaterialParameterType::Vec2; parameter.Value = value; return true; }
			return false;
		}
		if (!node.IsSequence()) return false;
		if (node.size() == 2) { parameter.Type = MaterialParameterType::Vec2; parameter.Value = glm::vec2(node[0].as<float>(), node[1].as<float>()); return true; }
		if (node.size() == 3) { parameter.Type = MaterialParameterType::Vec3; parameter.Value = glm::vec3(node[0].as<float>(), node[1].as<float>(), node[2].as<float>()); return true; }
		if (node.size() == 4) { parameter.Type = MaterialParameterType::Vec4; parameter.Value = glm::vec4(node[0].as<float>(), node[1].as<float>(), node[2].as<float>(), node[3].as<float>()); return true; }
		return false;
	}

	HE::ResultEnvelope MakeSourceFailure(const std::filesystem::path& path, std::string message) {
		auto result = HE::ResultEnvelope::Failure("asset.material_source.load", path.generic_string(), message);
		result.AddDetail({ HE::DiagnosticSeverity::Error, "asset.material_source.invalid", std::move(message), path.generic_string() });
		return result;
	}
}

namespace HE::Rendering {
	ResultEnvelope LoadMaterialSourceData(
		const std::filesystem::path& path,
		MaterialSourceData& outData) {
		outData = {};
		try {
			const auto root = YAML::LoadFile(path.string());
			if (!root.IsMap() || !root["name"] || !root["shader_guid"] || !root["parameters"]) {
				return MakeSourceFailure(path, "Material source is missing required fields");
			}

			outData.Name = root["name"].as<std::string>();
			outData.Type = MaterialType::Custom;
			if (const auto materialType = root["material_type"]; materialType && !ParseMaterialType(materialType.as<std::string>(), outData.Type)) {
				return MakeSourceFailure(path, "Material type is invalid");
			}
			if (outData.Name.empty()) {
				return MakeSourceFailure(path, "Material name or type is invalid");
			}
			outData.ShaderGuid = root["shader_guid"].as<std::string>();
			if (outData.ShaderGuid.empty()) return MakeSourceFailure(path, "Material shader GUID is invalid");

			const auto parameters = root["parameters"];
			if (!parameters.IsMap()) {
				return MakeSourceFailure(path, "Material parameters must be a mapping");
			}
			for (const auto& entry : parameters) {
				const auto parameterName = entry.first.as<std::string>();
				const auto parameterNode = entry.second;
				if (parameterName.empty()) {
					return MakeSourceFailure(path, "Material parameter entry is invalid");
				}

				MaterialSourceParameter parameter;
				parameter.Name = parameterName;
				const bool legacyEntry = parameterNode.IsMap() && parameterNode["value_type"] && parameterNode["value"];
				if (legacyEntry
					? (!ParseParameterType(parameterNode["value_type"].as<std::string>(), parameter.Type) || !ParseParameterValue(parameterNode["value"], parameter.Type, parameter.Value))
					: !ParseUntypedParameterValue(parameterNode, parameter)) {
					return MakeSourceFailure(path, "Material parameter type or value is unsupported");
				}
				outData.Parameters.emplace(parameterName, std::move(parameter));
			}

			return ResultEnvelope::Success("asset.material_source.load", path.generic_string(), "Material source loaded");
		}
		catch (const YAML::Exception& error) {
			return MakeSourceFailure(path, error.what());
		}
	}
}
