#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "HuaEngine/Core/Assert.h"
#include "HuaEngine/Core/ResultEnvelope.h"
#include "HuaEngine/Core/Sha256.h"
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
		Sha256Digest ShaderInterfaceDigest{};
		uint64_t ShaderInterfaceSignature = 0;
		Sha256Digest MaterialDefinitionDigest{};
		uint64_t MaterialDefinitionSignature = 0;
		std::unordered_map<std::string, MaterialSourceParameter> Parameters;
	};

	ResultEnvelope LoadMaterialSourceData(
		const std::filesystem::path& path,
		MaterialSourceData& outData);
}
