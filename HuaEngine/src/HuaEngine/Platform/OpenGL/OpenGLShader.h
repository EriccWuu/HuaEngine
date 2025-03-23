#pragma once

#include <string>
#include "HuaEngine/Renderer/Shader.h"

namespace HE {
	class OpenGLShader : public Shader {
	public:
		OpenGLShader(const std::string& vertexSrc, const std::string& fragmentSrc);
		~OpenGLShader();
		virtual void Bind() override;
		virtual void Unbind() override;

	private:
		unsigned int m_Program;
		unsigned int m_FragShader, m_VertShader;
	};
}