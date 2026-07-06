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
		// Compatibility helpers for the OpenGL migration period. New rendering code should bind programs and parameters through CommandList.
		virtual void Bind() = 0;
		virtual void Unbind() = 0;
		// Temporary uniform API until MaterialBinding or parameter sets own shader parameters.
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
