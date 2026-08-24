#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Rendering/RHI/BindGroup.h"
#include "HuaEngine/Rendering/RHI/CommandSubmission.h"
#include "HuaEngine/Rendering/RHI/RenderTargetTypes.h"
#include "HuaEngine/Rendering/RHI/GpuBuffer.h"
#include "HuaEngine/Rendering/RHI/PipelineState.h"
#include "HuaEngine/Rendering/RHI/ResourceBarrier.h"
#include "HuaEngine/Rendering/RHI/RenderTarget.h"
#include "HuaEngine/Rendering/RHI/ShaderProgram.h"
#include "HuaEngine/Rendering/RHI/TextureResource.h"
#include "HuaEngine/Rendering/RHI/VertexBufferView.h"

namespace HE::Rendering {
	class CommandList;

	enum class RenderBackendType : uint8_t {
		OpenGL = 0,
		Vulkan,
		D3D12,
		Metal,
		Null
	};

	struct RenderDeviceDesc {
		RenderBackendType Backend = RenderBackendType::OpenGL;
		bool EnableDebug = false;
		bool EnableValidation = false;
	};

	struct RenderDeviceCapabilities {
		RenderBackendType Backend = RenderBackendType::OpenGL;
		std::string BackendName = "OpenGL";
		bool SupportsPipelineState = true;
		bool SupportsBindGroups = true;
		bool SupportsCommandSubmission = true;
		bool SupportsComputeQueue = false;
		bool SupportsCopyQueue = false;
		bool SupportsRenderGraphResources = true;
		uint32_t UniformBufferOffsetAlignment = 1;
		uint32_t MaxUniformBufferBindings = 0;
	};

	struct BufferTransferDesc {
		Ref<GpuBuffer> Buffer;
		uint32_t Offset = 0;
		std::vector<uint8_t> Data;
	};

	struct TextureTransferDesc {
		Ref<TextureResource> Texture;
		uint32_t MipLevel = 0;
		std::vector<uint8_t> Data;
	};

	struct TextureResolveDesc {
		Ref<TextureResource> Source;
		Ref<TextureResource> Destination;
		TextureSubresourceRange SourceSubresources;
		TextureSubresourceRange DestinationSubresources;
	};

	class RenderDevice {
	public:
		virtual ~RenderDevice() = default;

		virtual const RenderDeviceDesc& GetDesc() const = 0;
		virtual const RenderDeviceCapabilities& GetCapabilities() const = 0;
		virtual CommandList& GetImmediateCommandList() = 0;
		virtual Ref<CommandBuffer> CreateCommandBuffer(const CommandBufferDesc& desc) = 0;
		virtual RenderQueue& GetGraphicsQueue() = 0;
		virtual RenderQueue& GetComputeQueue() = 0;
		virtual RenderQueue& GetCopyQueue() = 0;

		virtual Ref<GpuBuffer> CreateBuffer(const GpuBufferDesc& desc, const void* initialData) = 0;
		virtual bool UploadBuffer(const BufferTransferDesc& desc) = 0;
		virtual bool ReadbackBuffer(const Ref<GpuBuffer>& buffer, uint32_t offset, uint32_t size, std::vector<uint8_t>& outData) = 0;
		virtual bool UploadTexture(const TextureTransferDesc& desc) = 0;
		virtual bool ReadbackTexture(const Ref<TextureResource>& texture, uint32_t mipLevel, std::vector<uint8_t>& outData) = 0;
		virtual bool ResolveTexture(const TextureResolveDesc& desc) = 0;
		virtual Ref<VertexBufferView> CreateVertexBufferView(const VertexBufferViewDesc& desc) = 0;
		virtual Ref<RenderTarget> CreateRenderTarget(const RenderTargetDesc& desc) = 0;
		virtual Ref<TextureResource> CreateTexture(const TextureDesc& desc) = 0;
		virtual Ref<TextureView> CreateTextureView(const TextureViewDesc& desc) = 0;
		virtual Ref<Sampler> CreateSampler(const SamplerDesc& desc) = 0;
		virtual Ref<ShaderProgram> CreateShaderProgram(const ShaderProgramDesc& desc) = 0;
		virtual Ref<PipelineState> CreatePipelineState(const PipelineStateDesc& desc) = 0;
		virtual Ref<BindGroupLayout> CreateBindGroupLayout(const BindGroupLayoutDesc& desc) = 0;
		virtual Ref<BindGroup> CreateBindGroup(const BindGroupDesc& desc) = 0;
	};
}
