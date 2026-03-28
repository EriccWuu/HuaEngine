#include "enginepch.h"
#include "RendererAPI.h"
#include "Texture.h"
#include "HuaEngine/Core/ResourcePaths.h"
#include "Platform/OpenGL/OpenGLTexture2D.h"

namespace HE::Rendering {
	Ref<Texture2D> Texture2D::Create(const std::string& path)
	{
		const auto resolvedPath = ResourcePaths::ResolveRuntimePath(path);
		switch (RendererAPI::GetAPI()) {
		case RendererAPI::API::None:
			HE_CORE_ASSERT(false, "API None is not supported!");
			return nullptr;
		case RendererAPI::API::OpenGL:
			return std::make_shared<OpenGLTexture2D>(resolvedPath.generic_string());
		}

		HE_CORE_ASSERT(false, "No matching graphic API!");
		return nullptr;
	}
}
