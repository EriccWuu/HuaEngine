#pragma once

#include <string>
#include "HuaEngine/Rendering/Shader/Shader.h"
#include "HuaEngine/Rendering/RHI/ShaderProgram.h"

namespace HE::Rendering {
	class OpenGLShader : public Shader {
	public:
		OpenGLShader(const std::string& vertexSrc, const std::string& fragmentSrc);
		OpenGLShader(const std::string& vertexPath, const std::string& fragmentPath, bool fromFile);
		OpenGLShader(const std::string& shaderPath); // For combined shader files
		explicit OpenGLShader(Ref<ShaderProgram> shaderProgram);
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
		Ref<ShaderProgram> GetShaderProgram() const override { return m_ShaderProgram; }

	private:
		void CreateShader(const std::string& vertexSrc, const std::string& fragmentSrc);
		std::string ReadFile(const std::string& filepath);
		std::unordered_map<unsigned int, std::string> PreprocessShader(const std::string& source);
		
		unsigned int m_Program = 0;
		unsigned int m_FragShader = 0, m_VertShader = 0;
		Ref<ShaderProgram> m_ShaderProgram;
	};
}
