#pragma once

#include <cstdint>
#include <string>

#include "glm/glm.hpp"

namespace HE::Rendering {
	struct ShaderProgramDesc {
		std::string VertexSource;
		std::string FragmentSource;
	};

	class ShaderProgram {
	public:
		virtual ~ShaderProgram() = default;

		virtual const ShaderProgramDesc& GetDesc() const = 0;
		// Temporary uniform API for the OpenGL migration period.
		// New material parameters should be submitted through CommandList::SetMaterialBinding.
		virtual void SetInt(const std::string& name, int value) = 0;
		virtual void SetIntArray(const std::string& name, int* values, uint32_t size) = 0;
		virtual void SetFloat(const std::string& name, float value) = 0;
		virtual void SetFloat2(const std::string& name, const glm::vec2 value) = 0;
		virtual void SetFloat3(const std::string& name, const glm::vec3 value) = 0;
		virtual void SetFloat4(const std::string& name, const glm::vec4 value) = 0;
		virtual void SetMat3(const std::string& name, const glm::mat3 value) = 0;
		virtual void SetMat4(const std::string& name, const glm::mat4 value) = 0;
	};
}
