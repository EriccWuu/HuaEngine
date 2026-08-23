#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "HuaEngine/Core/Assert.h"
#include "HuaEngine/Core/ResultEnvelope.h"
#include "HuaEngine/Rendering/Material/MaterialCore.h"

namespace HE::Rendering {
	using MaterialSourceParameterValue = std::variant<
		int,
		float,
		glm::vec2,
		glm::vec3,
		glm::vec4,
		glm::mat3,
		glm::mat4,
		std::string,
		std::vector<int>,
		std::vector<float>>;

	struct MaterialSourceParameter {
		std::string Name;
		MaterialParameterType Type = MaterialParameterType::Float;
		MaterialSourceParameterValue Value = 0.0f;
	};

	struct MaterialSourceData {
		std::string Name;
		MaterialType Type = MaterialType::Empty;
		std::string ShaderGuid;
		std::unordered_map<std::string, MaterialSourceParameter> Parameters;
		std::unordered_map<std::string, uint32_t> TextureSlots;
	};

	ResultEnvelope LoadMaterialSourceData(
		const std::filesystem::path& path,
		MaterialSourceData& outData);
}
