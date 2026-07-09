#pragma once

#include "HuaEngine/Rendering/RHI/CommandList.h"
#include "HuaEngine/Rendering/RHI/RenderDevice.h"

namespace HE::Rendering {
	class Camera;
	class MaterialBinding;
	class OpenGLRenderTargetStorage;
	class OpenGLShader;

	class OpenGLCommandList final : public CommandList {
	public:
		void BeginRenderTarget(RenderTarget& target) override;
		void ClearColor(const glm::vec4& color) override;
		void BeginFrame(Camera& camera) override;
		void SetShaderProgram(ShaderProgram& shaderProgram) override;
		void SetPipelineState(PipelineState& pipelineState) override;
		void SetVertexBufferView(VertexBufferView& vertexBufferView) override;
		void SetFrameBinding(const FrameBinding& binding) override;
		void SetMaterialBinding(const MaterialBinding& binding) override;
		void SetObjectBinding(const ObjectBinding& binding) override;
		void DrawIndexed(uint32_t indexCount) override;
		void EndFrame() override;
		void EndRenderTarget() override;

	private:
		RenderTarget* m_CurrentRenderTarget = nullptr;
		Camera* m_CurrentCamera = nullptr;
		ShaderProgram* m_CurrentShaderProgram = nullptr;
		PipelineState* m_CurrentPipelineState = nullptr;
		VertexBufferView* m_CurrentVertexBufferView = nullptr;
		FrameBinding m_CurrentFrameBinding;
		ObjectBinding m_CurrentObjectBinding;
		bool m_HasFrameBinding = false;
		bool m_HasObjectBinding = false;
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
		uint32_t GetRenderID() const;

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
		uint32_t GetRenderID() const override;
		uint32_t GetColorAttachment(uint32_t index = 0) const override;
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
		uint32_t GetRenderID() const override;
		uint32_t GetWidth() const override;
		uint32_t GetHeight() const override;
		void BindForCommandList(uint32_t slot = 0);

	private:
		TextureDesc m_Desc;
		uint32_t m_RenderID = 0;
		uint32_t m_Width = 0;
		uint32_t m_Height = 0;
	};

	class OpenGLShaderProgram final : public ShaderProgram {
	public:
		explicit OpenGLShaderProgram(const ShaderProgramDesc& desc);
		OpenGLShaderProgram(const OpenGLShaderProgram&) = delete;
		OpenGLShaderProgram& operator=(const OpenGLShaderProgram&) = delete;

		const ShaderProgramDesc& GetDesc() const override;
		void BindForCommandList();
		void UnbindForCommandList();
		void SetInt(const std::string& name, int value) override;
		void SetIntArray(const std::string& name, int* values, uint32_t size) override;
		void SetFloat(const std::string& name, float value) override;
		void SetFloat2(const std::string& name, const glm::vec2 value) override;
		void SetFloat3(const std::string& name, const glm::vec3 value) override;
		void SetFloat4(const std::string& name, const glm::vec4 value) override;
		void SetMat3(const std::string& name, const glm::mat3 value) override;
		void SetMat4(const std::string& name, const glm::mat4 value) override;

	private:
		ShaderProgramDesc m_Desc;
		Ref<OpenGLShader> m_Shader;
	};

	class OpenGLRenderDevice final : public RenderDevice {
	public:
		OpenGLRenderDevice();

		CommandList& GetImmediateCommandList() override;
		Ref<GpuBuffer> CreateBuffer(const GpuBufferDesc& desc, const void* initialData) override;
		Ref<VertexBufferView> CreateVertexBufferView(const VertexBufferViewDesc& desc) override;
		Ref<RenderTarget> CreateRenderTarget(const RenderTargetDesc& desc) override;
		Ref<TextureResource> CreateTexture(const TextureDesc& desc) override;
		Ref<ShaderProgram> CreateShaderProgram(const ShaderProgramDesc& desc) override;
		Ref<PipelineState> CreatePipelineState(const PipelineStateDesc& desc) override;

	private:
		OpenGLCommandList m_ImmediateCommandList;
	};
}
