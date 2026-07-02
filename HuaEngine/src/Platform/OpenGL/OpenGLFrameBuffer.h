#pragma once 

#include "HuaEngine/Rendering/FrameBuffer.h"

namespace HE::Rendering {
	class OpenGLFrameBuffer : public FrameBuffer {
	public:
		OpenGLFrameBuffer(const FrameBufferSpecification& specificationn);
		virtual ~OpenGLFrameBuffer();

		virtual void Bind() override;
		virtual void Unbind() override;

		virtual void Resize(uint32_t width, uint32_t height) override;

		virtual void ClearAttachment(uint32_t index, int value) override;
		virtual FrameBufferPixelRGBA8 ReadPixelRGBA8(uint32_t attachmentIndex, uint32_t x, uint32_t y) const override;

		virtual uint32_t GetRenderID() const override { return m_RenderID; }

		virtual uint32_t GetColorAttachment(uint32_t index = 0) const override { HE_CORE_ASSERT(index < m_ColorAttachments.size(), ""); return m_ColorAttachments[index]; };
		virtual const FrameBufferSpecification& GetSpecification() const override { return m_Specification; }

		void Invalidate();

	private:
		uint32_t m_RenderID;
		FrameBufferSpecification m_Specification;

		std::vector<FrameBufferTextureSpecification> m_ColorAttachmentSpecifications;
		FrameBufferTextureSpecification m_DepthAttachmentSpecification = { FrameBufferTextureFormat::None };

		std::vector<uint32_t> m_ColorAttachments;
		uint32_t m_DepthAttachment;
	};
}
