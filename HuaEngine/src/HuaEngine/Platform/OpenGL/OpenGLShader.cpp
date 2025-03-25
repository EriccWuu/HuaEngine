#include "enginepch.h"
#include "OpenGLShader.h"
#include "glad/glad.h"

namespace HE {
	OpenGLShader::OpenGLShader(const std::string& vertexSrc, const std::string& fragmentSrc) {
		// Create an empty vertex shader handle
		GLuint m_VertShader = glCreateShader(GL_VERTEX_SHADER);

		// Send the vertex shader source code to GL
		// Note that std::string's .c_str is NULL character terminated.
		const GLchar* source = (const GLchar*)vertexSrc.c_str();
		glShaderSource(m_VertShader, 1, &source, 0);

		// Compile the vertex shader
		glCompileShader(m_VertShader);

		GLint isCompiled = 0;
		glGetShaderiv(m_VertShader, GL_COMPILE_STATUS, &isCompiled);
		if (isCompiled == GL_FALSE)
		{
			GLint maxLength = 0;
			glGetShaderiv(m_VertShader, GL_INFO_LOG_LENGTH, &maxLength);

			// The maxLength includes the NULL character
			std::vector<GLchar> infoLog(maxLength);
			glGetShaderInfoLog(m_VertShader, maxLength, &maxLength, &infoLog[0]);

			// We don't need the shader anymore.
			glDeleteShader(m_VertShader);

			// Use the infoLog as you see fit.
			HE_CORE_ERROR("Vertex shader compilation error");
			HE_CORE_ERROR("{0}", infoLog.data());

			HE_CORE_ASSERT(isCompiled, "Failed to complie vertex shader");
		}

		// Create an empty fragment shader handle
		GLuint m_FragShader = glCreateShader(GL_FRAGMENT_SHADER);

		// Send the fragment shader source code to GL
		// Note that std::string's .c_str is NULL character terminated.
		source = (const GLchar*)fragmentSrc.c_str();
		glShaderSource(m_FragShader, 1, &source, 0);

		// Compile the fragment shader
		glCompileShader(m_FragShader);

		glGetShaderiv(m_FragShader, GL_COMPILE_STATUS, &isCompiled);
		if (isCompiled == GL_FALSE)
		{
			GLint maxLength = 0;
			glGetShaderiv(m_FragShader, GL_INFO_LOG_LENGTH, &maxLength);

			// The maxLength includes the NULL character
			std::vector<GLchar> infoLog(maxLength);
			glGetShaderInfoLog(m_FragShader, maxLength, &maxLength, &infoLog[0]);

			// We don't need the shader anymore.
			glDeleteShader(m_FragShader);
			// Either of them. Don't leak shaders.
			glDeleteShader(m_VertShader);

			// Use the infoLog as you see fit.
			HE_CORE_ERROR("Fragment shader compilation error");
			HE_CORE_ERROR("{0}", infoLog.data());

			HE_CORE_ASSERT(isCompiled, "Failed to complie Fragment shader");
		}

		// Vertex and fragment shaders are successfully compiled.
		// Now time to link them together into a program.
		// Get a program object.
		m_Program = glCreateProgram();

		// Attach our shaders to our program
		glAttachShader(m_Program, m_VertShader);
		glAttachShader(m_Program, m_FragShader);

		// Link our program
		glLinkProgram(m_Program);

		// Note the different functions here: glGetProgram* instead of glGetShader*.
		GLint isLinked = 0;
		glGetProgramiv(m_Program, GL_LINK_STATUS, (int*)&isLinked);
		if (isLinked == GL_FALSE)
		{
			GLint maxLength = 0;
			glGetProgramiv(m_Program, GL_INFO_LOG_LENGTH, &maxLength);

			// The maxLength includes the NULL character
			std::vector<GLchar> infoLog(maxLength);
			glGetProgramInfoLog(m_Program, maxLength, &maxLength, &infoLog[0]);

			// We don't need the program anymore.
			glDeleteProgram(m_Program);
			// Don't leak shaders either.
			glDeleteShader(m_VertShader);
			glDeleteShader(m_FragShader);

			// Use the infoLog as you see fit.
			// Use the infoLog as you see fit.
			HE_CORE_ERROR("Shader program link error");
			HE_CORE_ERROR("{0}", infoLog.data());

			HE_CORE_ASSERT(isCompiled, "Failed to link shaders");
		}

		// Always detach shaders after a successful link.
		glDetachShader(m_Program, m_VertShader);
		glDetachShader(m_Program, m_FragShader);
	}

	OpenGLShader::~OpenGLShader()
	{
		glDeleteShader(m_VertShader);
		glDeleteShader(m_FragShader);
	}

	void OpenGLShader::Bind() {
		glUseProgram(m_Program);
	}

	void OpenGLShader::Unbind() {

	}

	void OpenGLShader::UploadUniformInt(const std::string name, uint32_t value) {
		GLint location = glGetUniformLocation(m_Program, name.c_str());
		glUniform1i(location, value);
	}
}


