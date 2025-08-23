#pragma once
#include "glm/glm.hpp"

namespace HE {
	class Shader {
	public:
		virtual ~Shader() = default;
		virtual void Bind() = 0;
		virtual void Unbind() = 0;

		virtual void SetInt(const std::string& name, int value) = 0;
		virtual void SetIntArray(const std::string& name, int* values, uint32_t size) = 0;
		virtual void SetFloat(const std::string& name, float value) = 0;
		virtual void SetFloat2(const std::string& name, const glm::vec2 value) = 0;
		virtual void SetFloat3(const std::string& name, const glm::vec3 value) = 0;
		virtual void SetFloat4(const std::string& name, const glm::vec4 value) = 0;
		virtual void SetMat3(const std::string& name, const glm::mat3 value) = 0;
		virtual void SetMat4(const std::string& name, const glm::mat4 value) = 0;

		static Ref<Shader> Create(const std::string& vertSrc, const std::string& fragSrc);
		static Ref<Shader> CreateFromFile(const std::string& vertexPath, const std::string& fragmentPath);
		static Ref<Shader> CreateFromFile(const std::string& shaderPath); // For combined shader files

		std::string GetPath() { return m_Path; }

	protected:
		std::string m_Path;
	};
}