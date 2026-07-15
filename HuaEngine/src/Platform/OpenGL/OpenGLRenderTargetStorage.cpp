#include "enginepch.h"
#include "OpenGLRenderTargetStorage.h"

#include "glad/glad.h"

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

		static bool IsDepthFormat(RenderTargetTextureFormat format)
		{
			switch (format)
			{
				case RenderTargetTextureFormat::DEPTH24_STENCIL8:  return true;
			}

			return false;
		}

		static GLenum TextureFormatToGL(RenderTargetTextureFormat format)
		{
			switch (format)
			{
				case RenderTargetTextureFormat::RGBA8:       return GL_RGBA8;
				case RenderTargetTextureFormat::RED_INTEGER: return GL_RED_INTEGER;
			}

			HE_CORE_ASSERT(false, "Unkonwn texture format");
			return 0;
		}
	}

	OpenGLRenderTargetStorage::OpenGLRenderTargetStorage(const RenderTargetSpecification& specificationn)
		: m_Specification(specificationn) {
		for (auto spec : m_Specification.Attachments.Attachments) {
			if (!Utils::IsDepthFormat(spec.Format))
				m_ColorAttachmentSpecifications.emplace_back(spec);
			else
				m_DepthAttachmentSpecification = spec;
		}

		Invalidate();
	}

	OpenGLRenderTargetStorage::~OpenGLRenderTargetStorage() {
		glDeleteFramebuffers(1, &m_RenderID);
		glDeleteTextures(m_ColorAttachments.size(), m_ColorAttachments.data());
		glDeleteTextures(1, &m_DepthAttachment);
	}

	void OpenGLRenderTargetStorage::BeginForCommandList() {
		glBindFramebuffer(GL_FRAMEBUFFER, m_RenderID);
		glViewport(0, 0, m_Specification.Width, m_Specification.Height);
	}

	void OpenGLRenderTargetStorage::EndForCommandList() {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void OpenGLRenderTargetStorage::Resize(uint32_t width, uint32_t height) {
		m_Specification.Width = width;
		m_Specification.Height = height;
		Invalidate();
	}

	void OpenGLRenderTargetStorage::ClearAttachment(uint32_t index, int value) {
		HE_CORE_ASSERT(index < m_ColorAttachments.size(), "Color attahcment index out of range");
		auto& spec = m_ColorAttachmentSpecifications[index];
		glClearTexImage(m_ColorAttachments[index], 0, Utils::TextureFormatToGL(spec.Format), GL_INT, &value);
	}

	RenderTargetPixelRGBA8 OpenGLRenderTargetStorage::ReadPixelRGBA8(uint32_t attachmentIndex, uint32_t x, uint32_t y) const {
		HE_CORE_ASSERT(attachmentIndex < m_ColorAttachments.size(), "Color attachment index out of range");
		HE_CORE_ASSERT(x < m_Specification.Width && y < m_Specification.Height, "Framebuffer pixel coordinates out of bounds");
		HE_CORE_ASSERT(m_ColorAttachmentSpecifications[attachmentIndex].Format == RenderTargetTextureFormat::RGBA8, "ReadPixelRGBA8 requires an RGBA8 attachment");

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

	uint32_t OpenGLRenderTargetStorage::GetRenderID() const {
		return m_RenderID;
	}

	uint32_t OpenGLRenderTargetStorage::GetColorAttachment(uint32_t index) const {
		HE_CORE_ASSERT(index < m_ColorAttachments.size(), "");
		return m_ColorAttachments[index];
	}

	uint32_t OpenGLRenderTargetStorage::GetDepthAttachment() const {
		return m_DepthAttachment;
	}

	const RenderTargetSpecification& OpenGLRenderTargetStorage::GetSpecification() const {
		return m_Specification;
	}

	void OpenGLRenderTargetStorage::Invalidate() {
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
					case RenderTargetTextureFormat::RGBA8:
						Utils::AttachColorTexture((uint32_t)m_ColorAttachments[i], m_Specification.Samples, GL_RGBA8, GL_RGBA, m_Specification.Width, m_Specification.Height, i);
						break;

					case RenderTargetTextureFormat::RED_INTEGER:
						Utils::AttachColorTexture((uint32_t)m_ColorAttachments[i], m_Specification.Samples, GL_R32I, GL_RED_INTEGER, m_Specification.Width, m_Specification.Height, i);
						break;
				}
			}
		}

		// Depth attachment
		if (m_DepthAttachmentSpecification.Format != RenderTargetTextureFormat::None) {
			glCreateTextures(target, 1, &m_DepthAttachment);
			glBindTexture(target, m_DepthAttachment);
			switch (m_DepthAttachmentSpecification.Format) {
				case RenderTargetTextureFormat::DEPTH24_STENCIL8:
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
