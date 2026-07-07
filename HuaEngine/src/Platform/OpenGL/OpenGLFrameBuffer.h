#pragma once 

#include "HuaEngine/Rendering/FrameBuffer.h"
#include "HuaEngine/Rendering/RHI/RenderTarget.h"

namespace HE::Rendering {
	class OpenGLFrameBuffer : public FrameBuffer {
	public:
		OpenGLFrameBuffer(const FrameBufferSpecification& specificationn);
		explicit OpenGLFrameBuffer(Ref<RenderTarget> renderTarget);
		virtual ~OpenGLFrameBuffer();

		void BeginForCommandList();
		void EndForCommandList();

		virtual void Resize(uint32_t width, uint32_t height) override;

		virtual void ClearAttachment(uint32_t index, int value) override;
		virtual FrameBufferPixelRGBA8 ReadPixelRGBA8(uint32_t attachmentIndex, uint32_t x, uint32_t y) const override;

		virtual uint32_t GetRenderID() const override;

		virtual uint32_t GetColorAttachment(uint32_t index = 0) const override;
		Ref<RenderTarget> GetRenderTarget() const override { return m_RenderTarget; }
		virtual const FrameBufferSpecification& GetSpecification() const override;

		void Invalidate();

	private:
		uint32_t m_RenderID = 0;
		FrameBufferSpecification m_Specification;
		Ref<RenderTarget> m_RenderTarget;

		std::vector<FrameBufferTextureSpecification> m_ColorAttachmentSpecifications;
		FrameBufferTextureSpecification m_DepthAttachmentSpecification = { FrameBufferTextureFormat::None };

		std::vector<uint32_t> m_ColorAttachments;
		uint32_t m_DepthAttachment = 0;
	};
}
