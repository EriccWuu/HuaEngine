#include "enginepch.h"
#include "ShaderProgramLoader.h"

#include "HuaEngine/Core/ResourcePaths.h"
#include "HuaEngine/Rendering/RHI/RenderHardwareInterface.h"

#include <fstream>
#include <sstream>
#include <unordered_map>

namespace HE::Rendering {
	namespace {
		enum class ShaderStage {
			Vertex,
			Fragment
		};

		std::string ReadFile(const std::string& filepath) {
			const auto resolvedPath = HE::ResourcePaths::ResolveRuntimePath(filepath);
			std::ifstream file(resolvedPath);
			if (!file.is_open()) {
				HE_CORE_ERROR("Failed to open shader file: {0}", resolvedPath.generic_string());
				return "";
			}

			std::stringstream stringStream;
			stringStream << file.rdbuf();
			return stringStream.str();
		}

		std::unordered_map<ShaderStage, std::string> PreprocessShader(const std::string& source) {
			std::unordered_map<ShaderStage, std::string> shaderSources;

			const char* typeToken = "#type";
			const size_t typeTokenLength = strlen(typeToken);
			size_t pos = source.find(typeToken, 0);
			while (pos != std::string::npos) {
				const size_t eol = source.find_first_of("\r\n", pos);
				HE_CORE_ASSERT(eol != std::string::npos, "Shader type declaration syntax error");

				const size_t begin = pos + typeTokenLength + 1;
				const std::string type = source.substr(begin, eol - begin);
				HE_CORE_ASSERT(type == "vertex" || type == "fragment" || type == "pixel", "Invalid shader type specified");

				const size_t nextLinePos = source.find_first_not_of("\r\n", eol);
				pos = source.find(typeToken, nextLinePos);
				const ShaderStage shaderStage = (type == "vertex") ? ShaderStage::Vertex : ShaderStage::Fragment;
				shaderSources[shaderStage] = source.substr(
					nextLinePos,
					pos - (nextLinePos == std::string::npos ? source.size() - 1 : nextLinePos));
			}

			return shaderSources;
		}
	}

	Ref<ShaderProgram> ShaderProgramLoader::CreateFromSource(const std::string& vertexSource, const std::string& fragmentSource) {
		return RenderHardwareInterface::GetDevice().CreateShaderProgram({
			.VertexSource = vertexSource,
			.FragmentSource = fragmentSource
		});
	}

	Ref<ShaderProgram> ShaderProgramLoader::CreateFromFile(const std::string& vertexPath, const std::string& fragmentPath) {
		const std::string vertexSource = ReadFile(vertexPath);
		const std::string fragmentSource = ReadFile(fragmentPath);

		if (vertexSource.empty() || fragmentSource.empty()) {
			HE_CORE_ERROR("Failed to read shader files");
			return nullptr;
		}

		return CreateFromSource(vertexSource, fragmentSource);
	}

	Ref<ShaderProgram> ShaderProgramLoader::CreateFromFile(const std::string& shaderPath) {
		const auto resolvedPath = HE::ResourcePaths::ResolveRuntimePath(shaderPath);
		const std::string source = ReadFile(resolvedPath.generic_string());
		if (source.empty()) {
			HE_CORE_ERROR("Failed to read shader file: {0}", resolvedPath.generic_string());
			return nullptr;
		}

		const auto shaderSources = PreprocessShader(source);
		HE_CORE_ASSERT(shaderSources.size() == 2, "Only vertex and fragment shaders are supported");

		return CreateFromSource(
			shaderSources.at(ShaderStage::Vertex),
			shaderSources.at(ShaderStage::Fragment));
	}
}
