#pragma once

#include "HuaEngine/Rendering/RHI/CommandList.h"
#include "HuaEngine/Rendering/RHI/RenderDevice.h"

namespace HE::Rendering {
	class Camera;
	class FrameBuffer;
	class MaterialBinding;
	class OpenGLFrameBuffer;
	class OpenGLShader;
	class OpenGLTexture2D;

	class OpenGLCommandList final : public CommandList {
	public:
		void BeginRenderTarget(FrameBuffer& target) override;
		void BeginRenderTarget(RenderTarget& target) override;
		void ClearColor(const glm::vec4& color) override;
		void BeginFrame(Camera& camera) override;
		void SetShaderProgram(ShaderProgram& shaderProgram) override;
		void SetVertexBufferView(VertexBufferView& vertexBufferView) override;
		void SetTexture(uint32_t slot, TextureResource& texture) override;
		void SetMat4(const std::string& name, const glm::mat4& value) override;
		void SetMaterialBinding(const MaterialBinding& binding) override;
		void DrawIndexed(uint32_t indexCount, const glm::mat4& transform) override;
		void DrawIndexed(MaterialInstance& material, VertexArray& vertexArray, const glm::mat4& transform) override;
		void EndFrame() override;
		void EndRenderTarget() override;

	private:
		FrameBuffer* m_CurrentLegacyTarget = nullptr;
		RenderTarget* m_CurrentRenderTarget = nullptr;
		Camera* m_CurrentCamera = nullptr;
		ShaderProgram* m_CurrentShaderProgram = nullptr;
		VertexBufferView* m_CurrentVertexBufferView = nullptr;
	};

	class OpenGLGpuBuffer final : public GpuBuffer {
	public:
		OpenGLGpuBuffer(const GpuBufferDesc& desc, const void* initialData);
		~OpenGLGpuBuffer() override;
		OpenGLGpuBuffer(const OpenGLGpuBuffer&) = delete;
		OpenGLGpuBuffer& operator=(const OpenGLGpuBuffer&) = delete;

		const GpuBufferDesc& GetDesc() const override;
		void Bind() const override;
		void Unbind() const override;
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
		void Bind() override;
		void Unbind() override;

	private:
		VertexBufferViewDesc m_Desc;
		uint32_t m_RenderID = 0;
	};

	class OpenGLRenderTarget final : public RenderTarget {
	public:
		explicit OpenGLRenderTarget(const RenderTargetDesc& desc);
		OpenGLRenderTarget(const OpenGLRenderTarget&) = delete;
		OpenGLRenderTarget& operator=(const OpenGLRenderTarget&) = delete;

		const RenderTargetDesc& GetDesc() const override;
		void Bind() override;
		void Unbind() override;
		void Resize(uint32_t width, uint32_t height) override;
		void ClearAttachment(uint32_t index, int value) override;
		FrameBufferPixelRGBA8 ReadPixelRGBA8(uint32_t attachmentIndex, uint32_t x, uint32_t y) const override;
		uint32_t GetRenderID() const override;
		uint32_t GetColorAttachment(uint32_t index = 0) const override;
		const FrameBufferSpecification& GetSpecification() const override;

	private:
		RenderTargetDesc m_Desc;
		Ref<OpenGLFrameBuffer> m_FrameBuffer;
	};

	class OpenGLTextureResource final : public TextureResource {
	public:
		explicit OpenGLTextureResource(const TextureDesc& desc);
		OpenGLTextureResource(const OpenGLTextureResource&) = delete;
		OpenGLTextureResource& operator=(const OpenGLTextureResource&) = delete;

		const TextureDesc& GetDesc() const override;
		uint32_t GetRenderID() const override;
		uint32_t GetWidth() const override;
		uint32_t GetHeight() const override;
		void Bind(uint32_t slot = 0) override;

	private:
		TextureDesc m_Desc;
		Ref<OpenGLTexture2D> m_Texture;
	};

	class OpenGLShaderProgram final : public ShaderProgram {
	public:
		explicit OpenGLShaderProgram(const ShaderProgramDesc& desc);
		OpenGLShaderProgram(const OpenGLShaderProgram&) = delete;
		OpenGLShaderProgram& operator=(const OpenGLShaderProgram&) = delete;

		const ShaderProgramDesc& GetDesc() const override;
		void Bind() override;
		void Unbind() override;
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
		Ref<GpuBuffer> CreateVertexBuffer(float* vertices, uint32_t size) override;
		Ref<GpuBuffer> CreateIndexBuffer(uint32_t* indices, uint32_t count) override;

	private:
		OpenGLCommandList m_ImmediateCommandList;
	};
}
