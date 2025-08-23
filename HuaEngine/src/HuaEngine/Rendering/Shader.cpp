#include "enginepch.h"
#include "Shader.h"
#include "HuaEngine/Rendering/RendererAPI.h"
#include "Platform/OpenGL/OpenGLShader.h"
#include <fstream>
#include <sstream>
#include <unordered_map>
#include "glad/glad.h"

namespace HE {
	
	static std::string ReadFile(const std::string& filepath) {
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

	static std::unordered_map<GLenum, std::string> PreprocessShader(const std::string& source) {
		std::unordered_map<GLenum, std::string> shaderSources;

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
			GLenum shaderType = (type == "vertex") ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER;
			shaderSources[shaderType] = source.substr(nextLinePos, 
				pos - (nextLinePos == std::string::npos ? source.size() - 1 : nextLinePos));
		}

		return shaderSources;
	}

	Ref<Shader> Shader::Create(const std::string& vertSrc, const std::string& fragSrc)
	{
		switch (RendererAPI::GetAPI()) {
		case RendererAPI::API::None:
			HE_CORE_ASSERT(false, "API None is not supported!");
			return nullptr;
		case RendererAPI::API::OpenGL:
			return std::make_shared<OpenGLShader>(vertSrc, fragSrc);
		}

		HE_CORE_ASSERT(false, "No matching graphic API!");
		return nullptr;
	}

	Ref<Shader> Shader::CreateFromFile(const std::string& vertexPath, const std::string& fragmentPath)
	{
		std::string vertexSource = ReadFile(vertexPath);
		std::string fragmentSource = ReadFile(fragmentPath);

		if (vertexSource.empty() || fragmentSource.empty()) {
			HE_CORE_ERROR("Failed to read shader files");
			return nullptr;
		}

		return Create(vertexSource, fragmentSource);
	}

	Ref<Shader> Shader::CreateFromFile(const std::string& shaderPath)
	{
		std::string source = ReadFile(shaderPath);
		if (source.empty()) {
			HE_CORE_ERROR("Failed to read shader file: {0}", shaderPath);
			return nullptr;
		}

		auto shaderSources = PreprocessShader(source);
		HE_CORE_ASSERT(shaderSources.size() == 2, "Only vertex and fragment shaders are supported");

		std::string vertexSource = shaderSources[GL_VERTEX_SHADER];
		std::string fragmentSource = shaderSources[GL_FRAGMENT_SHADER];

		return Create(vertexSource, fragmentSource);
	}
}