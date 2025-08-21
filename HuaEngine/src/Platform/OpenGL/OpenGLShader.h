#pragma once

#include <string>
#include "HuaEngine/Rendering/Shader.h"

namespace HE {
	class OpenGLShader : public Shader {
	public:
		OpenGLShader(const std::string& vertexSrc, const std::string& fragmentSrc);
		~OpenGLShader();
		virtual void Bind() override;
		virtual void Unbind() override;

		virtual void SetInt(const std::string& name, int value) override;
		virtual void SetIntArray(const std::string& name, int* values, uint32_t size) override;
		virtual void SetFloat(const std::string& name, float value) override;
		virtual void SetFloat2(const std::string& name, const glm::vec2 value) override;
		virtual void SetFloat3(const std::string& name, const glm::vec3 value) override;
		virtual void SetFloat4(const std::string& name, const glm::vec4 value) override;
		virtual void SetMat3(const std::string& name, const glm::mat3 value) override;
		virtual void SetMat4(const std::string& name, const glm::mat4 value) override;

	private:
		unsigned int m_Program;
		unsigned int m_FragShader, m_VertShader;
	};
}