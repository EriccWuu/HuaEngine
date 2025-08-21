#include "enginepch.h"
#include "Shader.h"
#include "HuaEngine/Rendering/RendererAPI.h"
#include "Platform/OpenGL/OpenGLShader.h"

namespace HE {
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
}