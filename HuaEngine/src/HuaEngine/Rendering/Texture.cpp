#include "enginepch.h"
#include "Texture.h"
#include "HuaEngine/Core/ResourcePaths.h"
#include "HuaEngine/Rendering/RHI/RenderHardwareInterface.h"
#include "Platform/OpenGL/OpenGLTexture2D.h"

namespace HE::Rendering {
	Ref<Texture2D> Texture2D::Create(const std::string& path)
	{
		const auto resolvedPath = ResourcePaths::ResolveRuntimePath(path);
		auto textureResource = RenderHardwareInterface::GetDevice().CreateTexture({ .SourcePath = resolvedPath.generic_string() });
		HE_CORE_ASSERT(textureResource, "Texture2D::Create failed to create texture resource");
		return CreateRef<OpenGLTexture2D>(textureResource);
	}
}
