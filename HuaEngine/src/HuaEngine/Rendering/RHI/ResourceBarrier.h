#pragma once

#include <cstdint>

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Rendering/RHI/TextureResource.h"

namespace HE::Rendering {
	class TextureResource;
	struct TextureSubresourceRange {
		uint32_t BaseMipLevel = 0;
		uint32_t MipLevelCount = 1;
		uint32_t BaseArrayLayer = 0;
		uint32_t ArrayLayerCount = 1;
		TextureAspect Aspect = TextureAspect::Color;
	};

	enum class ResourceState : uint32_t {
		Undefined = 0,
		RenderTarget,
		DepthStencilWrite,
		ShaderRead,
		CopySrc,
		CopyDst,
		VertexBuffer,
		IndexBuffer,
		Present
	};

	struct ResourceBarrier {
		Ref<TextureResource> Texture;
		ResourceState Before = ResourceState::Undefined;
		ResourceState After = ResourceState::Undefined;
		TextureSubresourceRange Subresources;
	};
}
