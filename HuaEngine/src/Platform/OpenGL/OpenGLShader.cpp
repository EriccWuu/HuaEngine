#include "enginepch.h"
#include "OpenGLShader.h"
#include "glad/glad.h"
#include "glm/glm.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace HE::Rendering {
	OpenGLShader::OpenGLShader(const std::string& vertexSrc, const std::string& fragmentSrc) {
		CreateShader(vertexSrc, fragmentSrc);
	}

	OpenGLShader::OpenGLShader(const std::string& vertexPath, const std::string& fragmentPath, bool fromFile) {
		std::string vertexSource = ReadFile(vertexPath);
		std::string fragmentSource = ReadFile(fragmentPath);
		
		if (vertexSource.empty() || fragmentSource.empty()) {
			HE_CORE_ERROR("Failed to read shader files");
			return;
		}
		
		CreateShader(vertexSource, fragmentSource);
	}

	OpenGLShader::OpenGLShader(const std::string& shaderPath) {
		std::string source = ReadFile(shaderPath);
		if (source.empty()) {
			HE_CORE_ERROR("Failed to read shader file: {0}", shaderPath);
			return;
		}

		auto shaderSources = PreprocessShader(source);
		HE_CORE_ASSERT(shaderSources.size() == 2, "Only vertex and fragment shaders are supported");

		std::string vertexSource = shaderSources[GL_VERTEX_SHADER];
		std::string fragmentSource = shaderSources[GL_FRAGMENT_SHADER];

		CreateShader(vertexSource, fragmentSource);
	}

	void OpenGLShader::CreateShader(const std::string& vertexSrc, const std::string& fragmentSrc) {
		// Create an empty vertex shader handle
		m_VertShader = glCreateShader(GL_VERTEX_SHADER);

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
		m_FragShader = glCreateShader(GL_FRAGMENT_SHADER);

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

	std::string OpenGLShader::ReadFile(const std::string& filepath) {
		std::ifstream file(filepath);
		if (!file.is_open()) {
			HE_CORE_ERROR("Failed to open file: {0}", filepath);
			return "";
		}

		std::stringstream stringStream;
		stringStream << file.rdbuf();
		file.close();
		return stringStream.str();
	}

	std::unordered_map<unsigned int, std::string> OpenGLShader::PreprocessShader(const std::string& source) {
		std::unordered_map<unsigned int, std::string> shaderSources;

		const char* typeToken = "#type";
		size_t typeTokenLength = strlen(typeToken);
		size_t pos = source.find(typeToken, 0);
		while (pos != std::string::npos) {
			size_t eol = source.find_first_of("\r\n", pos);
			HE_CORE_ASSERT(eol != std::string::npos, "Syntax error");
			size_t begin = pos + typeTokenLength + 1;
			std::string type = source.substr(begin, eol - begin);
			HE_CORE_ASSERT(type == "vertex" || type == "fragment" || type == "pixel", "Invalid shader type specified");

			size_t nextLinePos = source.find_first_not_of("\r\n", eol);
			pos = source.find(typeToken, nextLinePos);
			unsigned int shaderType = (type == "vertex") ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER;
			shaderSources[shaderType] = source.substr(nextLinePos, 
				pos - (nextLinePos == std::string::npos ? source.size() - 1 : nextLinePos));
		}

		return shaderSources;
	}

	OpenGLShader::~OpenGLShader()
	{
		glDeleteProgram(m_Program);
		glDeleteShader(m_VertShader);
		glDeleteShader(m_FragShader);
	}

	void OpenGLShader::Bind() {
		glUseProgram(m_Program);
	}

	void OpenGLShader::Unbind() {

	}

	void OpenGLShader::SetInt(const std::string& name, int value) {
		GLint location = glGetUniformLocation(m_Program, name.c_str());
		glUniform1i(location, value);
	}

	void OpenGLShader::SetIntArray(const std::string& name, int* values, uint32_t size) {
		GLint location = glGetUniformLocation(m_Program, name.c_str());
		glUniform1iv(location, size, values);
	}

	void OpenGLShader::SetFloat(const std::string& name, float value) {
		GLint location = glGetUniformLocation(m_Program, name.c_str());
		glUniform1f(location, value);
	}

	void OpenGLShader::SetFloat2(const std::string& name, const glm::vec2 value) {
		GLint location = glGetUniformLocation(m_Program, name.c_str());
		glUniform2f(location, value.x, value.y);
	}

	void OpenGLShader::SetFloat3(const std::string& name, const glm::vec3 value) {
		GLint location = glGetUniformLocation(m_Program, name.c_str());
		glUniform3f(location, value.x, value.y, value.z);
	}

	void OpenGLShader::SetFloat4(const std::string& name, const glm::vec4 value) {
		GLint location = glGetUniformLocation(m_Program, name.c_str());
		glUniform4f(location, value.x, value.y, value.z, value.w);
	}

	void OpenGLShader::SetMat3(const std::string& name, const glm::mat3 value) {
		GLint location = glGetUniformLocation(m_Program, name.c_str());
		glUniformMatrix3fv(location, 1, GL_FALSE, glm::value_ptr(value));
	}

	void OpenGLShader::SetMat4(const std::string& name, const glm::mat4 value) {
		GLint location = glGetUniformLocation(m_Program, name.c_str());
		glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value));
	}
}


