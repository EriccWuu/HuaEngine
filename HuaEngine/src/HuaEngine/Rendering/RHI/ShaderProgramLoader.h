#pragma once

#include <string>

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Rendering/RHI/ShaderProgram.h"

namespace HE::Rendering {
	class ShaderProgramLoader {
	public:
		static Ref<ShaderProgram> CreateFromSource(const std::string& vertexSource, const std::string& fragmentSource);
		static Ref<ShaderProgram> CreateFromFile(const std::string& vertexPath, const std::string& fragmentPath);
		static Ref<ShaderProgram> CreateFromFile(const std::string& shaderPath);
	};
}
