#pragma once 

#include "HuaEngine/Rendering/FrameBuffer.h"

namespace HE::Rendering {
	class OpenGLFrameBuffer {
	public:
		OpenGLFrameBuffer(const FrameBufferSpecification& specificationn);
		~OpenGLFrameBuffer();

		void BeginForCommandList();
		void EndForCommandList();

		void Resize(uint32_t width, uint32_t height);

		void ClearAttachment(uint32_t index, int value);
		FrameBufferPixelRGBA8 ReadPixelRGBA8(uint32_t attachmentIndex, uint32_t x, uint32_t y) const;

		uint32_t GetRenderID() const;

		uint32_t GetColorAttachment(uint32_t index = 0) const;
		const FrameBufferSpecification& GetSpecification() const;

		void Invalidate();

	private:
		uint32_t m_RenderID = 0;
		FrameBufferSpecification m_Specification;

		std::vector<FrameBufferTextureSpecification> m_ColorAttachmentSpecifications;
		FrameBufferTextureSpecification m_DepthAttachmentSpecification = { FrameBufferTextureFormat::None };

		std::vector<uint32_t> m_ColorAttachments;
		uint32_t m_DepthAttachment = 0;
	};
}
