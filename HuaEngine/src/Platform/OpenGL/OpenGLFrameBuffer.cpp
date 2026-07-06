#include "enginepch.h"
#include "OpenGLFrameBuffer.h"

#include "glad/glad.h"
#include <utility>

namespace HE::Rendering {
	namespace Utils {
		static GLenum TextureTarget(bool multisampled)
		{
			return multisampled ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
		}

		static void AttachColorTexture(uint32_t id, int samples, GLenum internalFormat, GLenum format, uint32_t width, uint32_t height, int index)
		{
			bool multisampled = samples > 1;
			if (multisampled)
			{
				glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, internalFormat, width, height, GL_FALSE);
			}
			else
			{
				glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, nullptr);

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			}

			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index, TextureTarget(multisampled), id, 0);
		}

		static void AttachDepthTexture(uint32_t id, int samples, GLenum format, GLenum attachmentType, uint32_t width, uint32_t height)
		{
			bool multisampled = samples > 1;
			if (multisampled)
			{
				glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, format, width, height, GL_FALSE);
			}
			else
			{
				glTexStorage2D(GL_TEXTURE_2D, 1, format, width, height);

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			}

			glFramebufferTexture2D(GL_FRAMEBUFFER, attachmentType, TextureTarget(multisampled), id, 0);
		}

		static bool IsDepthFormat(FrameBufferTextureFormat format)
		{
			switch (format)
			{
				case FrameBufferTextureFormat::DEPTH24_STENCIL8:  return true;
			}

			return false;
		}

		static GLenum TextureFormatToGL(FrameBufferTextureFormat format)
		{
			switch (format)
			{
				case FrameBufferTextureFormat::RGBA8:       return GL_RGBA8;
				case FrameBufferTextureFormat::RED_INTEGER: return GL_RED_INTEGER;
			}

			HE_CORE_ASSERT(false, "Unkonwn texture format");
			return 0;
		}
	}

	OpenGLFrameBuffer::OpenGLFrameBuffer(const FrameBufferSpecification& specificationn)
		: m_Specification(specificationn) {
		for (auto spec : m_Specification.Attachments.Attachments) {
			if (!Utils::IsDepthFormat(spec.Format))
				m_ColorAttachmentSpecifications.emplace_back(spec);
			else
				m_DepthAttachmentSpecification = spec;
		}

		Invalidate();
	}

	OpenGLFrameBuffer::OpenGLFrameBuffer(Ref<RenderTarget> renderTarget)
		: m_RenderTarget(std::move(renderTarget)) {
		HE_CORE_ASSERT(m_RenderTarget, "OpenGLFrameBuffer requires a render target");
	}

	OpenGLFrameBuffer::~OpenGLFrameBuffer() {
		if (m_RenderTarget) {
			return;
		}

		glDeleteFramebuffers(1, &m_RenderID);
		glDeleteTextures(m_ColorAttachments.size(), m_ColorAttachments.data());
		glDeleteTextures(1, &m_DepthAttachment);
	}

	void OpenGLFrameBuffer::Bind() {
		if (m_RenderTarget) {
			m_RenderTarget->Bind();
			return;
		}

		glBindFramebuffer(GL_FRAMEBUFFER, m_RenderID);
		glViewport(0, 0, m_Specification.Width, m_Specification.Height);
	}

	void OpenGLFrameBuffer::Unbind() {
		if (m_RenderTarget) {
			m_RenderTarget->Unbind();
			return;
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void OpenGLFrameBuffer::Resize(uint32_t width, uint32_t height) {
		if (m_RenderTarget) {
			m_RenderTarget->Resize(width, height);
			return;
		}

		m_Specification.Width = width;
		m_Specification.Height = height;
		Invalidate();
	}

	void OpenGLFrameBuffer::ClearAttachment(uint32_t index, int value) {
		if (m_RenderTarget) {
			m_RenderTarget->ClearAttachment(index, value);
			return;
		}

		HE_CORE_ASSERT(index < m_ColorAttachments.size(), "Color attahcment index out of range");
		auto& spec = m_ColorAttachmentSpecifications[index];
		glClearTexImage(m_ColorAttachments[index], 0, Utils::TextureFormatToGL(spec.Format), GL_INT, &value);
	}

	FrameBufferPixelRGBA8 OpenGLFrameBuffer::ReadPixelRGBA8(uint32_t attachmentIndex, uint32_t x, uint32_t y) const {
		if (m_RenderTarget) {
			return m_RenderTarget->ReadPixelRGBA8(attachmentIndex, x, y);
		}

		HE_CORE_ASSERT(attachmentIndex < m_ColorAttachments.size(), "Color attachment index out of range");
		HE_CORE_ASSERT(x < m_Specification.Width && y < m_Specification.Height, "Framebuffer pixel coordinates out of bounds");
		HE_CORE_ASSERT(m_ColorAttachmentSpecifications[attachmentIndex].Format == FrameBufferTextureFormat::RGBA8, "ReadPixelRGBA8 requires an RGBA8 attachment");

		GLint previousReadFramebuffer = 0;
		glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousReadFramebuffer);

		glBindFramebuffer(GL_READ_FRAMEBUFFER, m_RenderID);

		GLint framebufferReadBuffer = 0;
		glGetIntegerv(GL_READ_BUFFER, &framebufferReadBuffer);
		glReadBuffer(GL_COLOR_ATTACHMENT0 + attachmentIndex);

		uint8_t pixel[4] = {};
		glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);

		glReadBuffer(framebufferReadBuffer);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, previousReadFramebuffer);

		return { pixel[0], pixel[1], pixel[2], pixel[3] };
	}

	uint32_t OpenGLFrameBuffer::GetRenderID() const {
		if (m_RenderTarget) {
			return m_RenderTarget->GetRenderID();
		}

		return m_RenderID;
	}

	uint32_t OpenGLFrameBuffer::GetColorAttachment(uint32_t index) const {
		if (m_RenderTarget) {
			return m_RenderTarget->GetColorAttachment(index);
		}

		HE_CORE_ASSERT(index < m_ColorAttachments.size(), "");
		return m_ColorAttachments[index];
	}

	const FrameBufferSpecification& OpenGLFrameBuffer::GetSpecification() const {
		if (m_RenderTarget) {
			return m_RenderTarget->GetSpecification();
		}

		return m_Specification;
	}

	void OpenGLFrameBuffer::Invalidate() {
		HE_CORE_ASSERT(!m_RenderTarget, "Render target backed framebuffers cannot be invalidated directly");

		if (m_RenderID) {
			glDeleteFramebuffers(1, &m_RenderID);
			glDeleteTextures(m_ColorAttachments.size(), m_ColorAttachments.data());
			glDeleteTextures(1, &m_DepthAttachment);

			m_ColorAttachments.clear();
			m_DepthAttachment = 0;
		}

		glCreateFramebuffers(1, &m_RenderID);
		glBindFramebuffer(GL_FRAMEBUFFER, m_RenderID);

		bool multisample = m_Specification.Samples > 1;
		GLint target = Utils::TextureTarget(multisample);

		// Color attachments
		if (m_ColorAttachmentSpecifications.size()) {
			m_ColorAttachments.resize(m_ColorAttachmentSpecifications.size());
			
			glCreateTextures(target, m_ColorAttachments.size(), m_ColorAttachments.data());

			for (size_t i = 0; i < m_ColorAttachments.size(); i++) {
				glBindTexture(target, m_ColorAttachments[i]);
				switch (m_ColorAttachmentSpecifications[i].Format) {
					case FrameBufferTextureFormat::RGBA8:
						Utils::AttachColorTexture((uint32_t)m_ColorAttachments[i], m_Specification.Samples, GL_RGBA8, GL_RGBA, m_Specification.Width, m_Specification.Height, i);
						break;

					case FrameBufferTextureFormat::RED_INTEGER:
						Utils::AttachColorTexture((uint32_t)m_ColorAttachments[i], m_Specification.Samples, GL_R32I, GL_RED_INTEGER, m_Specification.Width, m_Specification.Height, i);
						break;
				}
			}
		}

		// Depth attachment
		if (m_DepthAttachmentSpecification.Format != FrameBufferTextureFormat::None) {
			glCreateTextures(target, 1, &m_DepthAttachment);
			glBindTexture(target, m_DepthAttachment);
			switch (m_DepthAttachmentSpecification.Format) {
				case FrameBufferTextureFormat::DEPTH24_STENCIL8:
					Utils::AttachDepthTexture(m_DepthAttachment, m_Specification.Samples, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL_ATTACHMENT, m_Specification.Width, m_Specification.Height);
					break;
			}
		}

		// Multi render target
		if (m_ColorAttachments.size() > 1)
		{
			HE_CORE_ASSERT(m_ColorAttachments.size() <= 4, "Render target size greater than 4!");
			GLenum buffers[4] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
			glDrawBuffers(m_ColorAttachments.size(), buffers);
		}
		else if (m_ColorAttachments.empty())
		{
			// Only depth-pass
			glDrawBuffer(GL_NONE);
		}

		HE_CORE_ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Framebuffer is incomplete!");

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}


}
