#pragma once

#include <cstdint>

#include "HuaEngine/Core/Core.h"

namespace HE::Rendering {
	class TextureResource;

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
	};
}
