#pragma once

#include <cstdint>
#include <string>

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Rendering/FrameBuffer.h"
#include "HuaEngine/Rendering/RHI/GpuBuffer.h"
#include "HuaEngine/Rendering/RHI/RenderTarget.h"
#include "HuaEngine/Rendering/RHI/ShaderProgram.h"
#include "HuaEngine/Rendering/RHI/TextureResource.h"
#include "HuaEngine/Rendering/RHI/VertexBufferView.h"

namespace HE::Rendering {
	class CommandList;

	class RenderDevice {
	public:
		virtual ~RenderDevice() = default;

		virtual CommandList& GetImmediateCommandList() = 0;

		virtual Ref<GpuBuffer> CreateBuffer(const GpuBufferDesc& desc, const void* initialData) = 0;
		virtual Ref<VertexBufferView> CreateVertexBufferView(const VertexBufferViewDesc& desc) = 0;
		virtual Ref<RenderTarget> CreateRenderTarget(const RenderTargetDesc& desc) = 0;
		virtual Ref<TextureResource> CreateTexture(const TextureDesc& desc) = 0;
		virtual Ref<ShaderProgram> CreateShaderProgram(const ShaderProgramDesc& desc) = 0;

		// Compatibility helpers for legacy VertexBuffer/IndexBuffer shells only.
		virtual Ref<GpuBuffer> CreateVertexBuffer(float* vertices, uint32_t size) = 0;
		virtual Ref<GpuBuffer> CreateIndexBuffer(uint32_t* indices, uint32_t count) = 0;
	};
}
