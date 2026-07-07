#pragma once

#include <string>
#include <unordered_map>

#include "glm/glm.hpp"
#include "HuaEngine/Core/Core.h"

namespace HE::Rendering {
	class OpenGLShader {
	public:
		OpenGLShader(const std::string& vertexSrc, const std::string& fragmentSrc);
		OpenGLShader(const std::string& vertexPath, const std::string& fragmentPath, bool fromFile);
		OpenGLShader(const std::string& shaderPath); // For combined shader files
		~OpenGLShader();
		
		void BindForCommandList();
		void UnbindForCommandList();

		void SetInt(const std::string& name, int value);
		void SetIntArray(const std::string& name, int* values, uint32_t size);
		void SetFloat(const std::string& name, float value);
		void SetFloat2(const std::string& name, const glm::vec2 value);
		void SetFloat3(const std::string& name, const glm::vec3 value);
		void SetFloat4(const std::string& name, const glm::vec4 value);
		void SetMat3(const std::string& name, const glm::mat3 value);
		void SetMat4(const std::string& name, const glm::mat4 value);

	private:
		void CreateShader(const std::string& vertexSrc, const std::string& fragmentSrc);
		std::string ReadFile(const std::string& filepath);
		std::unordered_map<unsigned int, std::string> PreprocessShader(const std::string& source);
		
		unsigned int m_Program = 0;
		unsigned int m_FragShader = 0, m_VertShader = 0;
	};
}
