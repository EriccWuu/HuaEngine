#pragma once

#include <functional>
#include <vector>

#include "HuaEngine/Rendering/RHI/CommandList.h"
#include "HuaEngine/Rendering/RHI/RenderDevice.h"

namespace HE::Rendering {
	class OpenGLRenderTargetStorage;
	class OpenGLShader;

	class OpenGLCommandList final : public CommandList {
	public:
		~OpenGLCommandList() override;

		void BeginRenderPass(const RenderPassDesc& desc) override;
		void EndRenderPass() override;
		void ResourceBarrier(const HE::Rendering::ResourceBarrier& barrier) override;

		void BeginRenderTarget(RenderTarget& target) override;
		void ClearColor(const glm::vec4& color) override;
		void BeginFrame() override;
		void SetPipelineState(PipelineState& pipelineState) override;
		void SetVertexBuffer(uint32_t slot, const VertexBufferBinding& binding) override;
		void SetIndexBuffer(const IndexBufferBinding& binding) override;
		void SetVertexBufferView(VertexBufferView& vertexBufferView) override;
		void SetBindGroup(uint32_t slot, BindGroup& bindGroup) override;
		void DrawIndexed(uint32_t indexCount) override;
		void EndFrame() override;
		void EndRenderTarget() override;

	private:
		void RebuildExplicitVertexArray();
		void ReleaseExplicitVertexArray();

		RenderTarget* m_CurrentRenderTarget = nullptr;
		ShaderProgram* m_CurrentShaderProgram = nullptr;
		PipelineState* m_CurrentPipelineState = nullptr;
		VertexBufferView* m_CurrentVertexBufferView = nullptr;
		std::vector<uint32_t> m_BoundBindGroupSlots;
		VertexBufferBinding m_CurrentVertexBufferBinding;
		IndexBufferBinding m_CurrentIndexBufferBinding;
		uint32_t m_ExplicitVertexArray = 0;
		bool m_HasExplicitVertexBuffer = false;
		bool m_HasExplicitIndexBuffer = false;
	};

	class OpenGLCommandBuffer final : public CommandBuffer {
	public:
		explicit OpenGLCommandBuffer(const CommandBufferDesc& desc);

		const CommandBufferDesc& GetDesc() const override;
		bool Begin() override;
		bool End() override;
		void Reset() override;
		bool IsRecording() const override;
		bool IsExecutable() const override;

		bool RecordBeginRenderPass(const RenderPassDesc& desc) override;
		bool RecordEndRenderPass() override;
		bool RecordSetPipelineState(PipelineState& pipelineState) override;
		bool RecordSetVertexBuffer(uint32_t slot, const VertexBufferBinding& binding) override;
		bool RecordSetIndexBuffer(const IndexBufferBinding& binding) override;
		bool RecordSetBindGroup(uint32_t slot, BindGroup& bindGroup) override;
		bool RecordDrawIndexed(uint32_t indexCount) override;
		void Replay(CommandList& commandList);

	private:
		using RecordedCommand = std::function<void(CommandList&)>;

		bool CanRecord() const;

		CommandBufferDesc m_Desc;
		std::vector<RecordedCommand> m_Commands;
		bool m_IsRecording = false;
		bool m_IsExecutable = false;
	};

	class OpenGLRenderQueue final : public RenderQueue {
	public:
		explicit OpenGLRenderQueue(CommandList* immediateCommandList = nullptr);

		QueueSubmitResult Submit(CommandBuffer& commandBuffer) override;
		Fence& GetTimelineFence() override;

	private:
		class OpenGLFence final : public Fence {
		public:
			uint64_t GetCompletedValue() const override { return m_CompletedValue; }
			void Signal(uint64_t value) { m_CompletedValue = value; }

		private:
			uint64_t m_CompletedValue = 0;
		};

		CommandList* m_ImmediateCommandList = nullptr;
		OpenGLFence m_TimelineFence;
		uint64_t m_NextSignalValue = 0;
	};

	class OpenGLGpuBuffer final : public GpuBuffer {
	public:
		OpenGLGpuBuffer(const GpuBufferDesc& desc, const void* initialData);
		~OpenGLGpuBuffer() override;
		OpenGLGpuBuffer(const OpenGLGpuBuffer&) = delete;
		OpenGLGpuBuffer& operator=(const OpenGLGpuBuffer&) = delete;

		const GpuBufferDesc& GetDesc() const override;
		void BindForCommandList() const;
		void UnbindForCommandList() const;

	private:
		GpuBufferDesc m_Desc;
		uint32_t m_RenderID = 0;
	};

	class OpenGLVertexBufferView final : public VertexBufferView {
	public:
		explicit OpenGLVertexBufferView(const VertexBufferViewDesc& desc);
		~OpenGLVertexBufferView() override;
		OpenGLVertexBufferView(const OpenGLVertexBufferView&) = delete;
		OpenGLVertexBufferView& operator=(const OpenGLVertexBufferView&) = delete;

		const VertexBufferViewDesc& GetDesc() const override;
		void BindForCommandList();
		void UnbindForCommandList();

	private:
		VertexBufferViewDesc m_Desc;
		uint32_t m_RenderID = 0;
	};

	class OpenGLBindGroupLayout final : public BindGroupLayout {
	public:
		explicit OpenGLBindGroupLayout(const BindGroupLayoutDesc& desc);

		const BindGroupLayoutDesc& GetDesc() const override;

	private:
		BindGroupLayoutDesc m_Desc;
	};

	class OpenGLBindGroup final : public BindGroup {
	public:
		explicit OpenGLBindGroup(const BindGroupDesc& desc);

		const BindGroupDesc& GetDesc() const override;

	private:
		BindGroupDesc m_Desc;
	};

	class OpenGLPipelineState final : public PipelineState {
	public:
		explicit OpenGLPipelineState(const PipelineStateDesc& desc);

		const PipelineStateDesc& GetDesc() const override;
		ShaderProgram& GetShaderProgram() const;
		PrimitiveTopology GetTopology() const;

	private:
		PipelineStateDesc m_Desc;
	};

	class OpenGLRenderTarget final : public RenderTarget {
	public:
		explicit OpenGLRenderTarget(const RenderTargetDesc& desc);
		OpenGLRenderTarget(const OpenGLRenderTarget&) = delete;
		OpenGLRenderTarget& operator=(const OpenGLRenderTarget&) = delete;

		const RenderTargetDesc& GetDesc() const override;
		void BeginForCommandList();
		void EndForCommandList();
		void Resize(uint32_t width, uint32_t height) override;
		void ClearAttachment(uint32_t index, int value) override;
		RenderTargetPixelRGBA8 ReadPixelRGBA8(uint32_t attachmentIndex, uint32_t x, uint32_t y) const override;
		RenderTargetColorAttachmentView GetColorAttachmentView(uint32_t index = 0) const override;
		const RenderTargetSpecification& GetSpecification() const override;

	private:
		RenderTargetDesc m_Desc;
		Ref<OpenGLRenderTargetStorage> m_BackendStorage;
	};

	class OpenGLTextureResource final : public TextureResource {
	public:
		explicit OpenGLTextureResource(const TextureDesc& desc);
		~OpenGLTextureResource() override;
		OpenGLTextureResource(const OpenGLTextureResource&) = delete;
		OpenGLTextureResource& operator=(const OpenGLTextureResource&) = delete;

		const TextureDesc& GetDesc() const override;
		uint32_t GetWidth() const override;
		uint32_t GetHeight() const override;
		void BindForCommandList(uint32_t slot = 0);

	private:
		TextureDesc m_Desc;
		uint32_t m_RenderID = 0;
		uint32_t m_Width = 0;
		uint32_t m_Height = 0;
	};

	class OpenGLTextureView final : public TextureView {
	public:
		explicit OpenGLTextureView(const TextureViewDesc& desc);

		const TextureViewDesc& GetDesc() const override;
		void BindForCommandList(uint32_t slot = 0);

	private:
		TextureViewDesc m_Desc;
	};

	class OpenGLSampler final : public Sampler {
	public:
		explicit OpenGLSampler(const SamplerDesc& desc);
		~OpenGLSampler() override;
		OpenGLSampler(const OpenGLSampler&) = delete;
		OpenGLSampler& operator=(const OpenGLSampler&) = delete;

		const SamplerDesc& GetDesc() const override;
		void BindForCommandList(uint32_t slot = 0);

	private:
		SamplerDesc m_Desc;
		uint32_t m_RenderID = 0;
	};

	class OpenGLShaderProgram final : public ShaderProgram {
	public:
		explicit OpenGLShaderProgram(const ShaderProgramDesc& desc);
		OpenGLShaderProgram(const OpenGLShaderProgram&) = delete;
		OpenGLShaderProgram& operator=(const OpenGLShaderProgram&) = delete;

		const ShaderProgramDesc& GetDesc() const override;
		void BindForCommandList();
		void UnbindForCommandList();
		void SetInt(const std::string& name, int value);
		void SetIntArray(const std::string& name, int* values, uint32_t size);
		void SetFloat(const std::string& name, float value);
		void SetFloat2(const std::string& name, const glm::vec2 value);
		void SetFloat3(const std::string& name, const glm::vec3 value);
		void SetFloat4(const std::string& name, const glm::vec4 value);
		void SetMat3(const std::string& name, const glm::mat3 value);
		void SetMat4(const std::string& name, const glm::mat4 value);

	private:
		ShaderProgramDesc m_Desc;
		Ref<OpenGLShader> m_Shader;
	};

	class OpenGLRenderDevice final : public RenderDevice {
	public:
		OpenGLRenderDevice();
		explicit OpenGLRenderDevice(const RenderDeviceDesc& desc);

		const RenderDeviceDesc& GetDesc() const override;
		const RenderDeviceCapabilities& GetCapabilities() const override;
		CommandList& GetImmediateCommandList() override;
		Ref<CommandBuffer> CreateCommandBuffer(const CommandBufferDesc& desc) override;
		RenderQueue& GetGraphicsQueue() override;
		Ref<GpuBuffer> CreateBuffer(const GpuBufferDesc& desc, const void* initialData) override;
		Ref<VertexBufferView> CreateVertexBufferView(const VertexBufferViewDesc& desc) override;
		Ref<RenderTarget> CreateRenderTarget(const RenderTargetDesc& desc) override;
		Ref<TextureResource> CreateTexture(const TextureDesc& desc) override;
		Ref<TextureView> CreateTextureView(const TextureViewDesc& desc) override;
		Ref<Sampler> CreateSampler(const SamplerDesc& desc) override;
		Ref<ShaderProgram> CreateShaderProgram(const ShaderProgramDesc& desc) override;
		Ref<PipelineState> CreatePipelineState(const PipelineStateDesc& desc) override;
		Ref<BindGroupLayout> CreateBindGroupLayout(const BindGroupLayoutDesc& desc) override;
		Ref<BindGroup> CreateBindGroup(const BindGroupDesc& desc) override;

	private:
		RenderDeviceDesc m_Desc;
		RenderDeviceCapabilities m_Capabilities;
		OpenGLCommandList m_ImmediateCommandList;
		OpenGLRenderQueue m_GraphicsQueue;
	};
}
