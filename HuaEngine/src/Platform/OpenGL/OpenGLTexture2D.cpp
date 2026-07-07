#include "enginepch.h"
#include "OpenGLTexture2D.h"
#include "stb_image.h"
#include "glad/glad.h"
#include "Platform/OpenGL/RHI/OpenGLRenderDevice.h"
#include <utility>

namespace HE::Rendering {
	OpenGLTexture2D::OpenGLTexture2D(const std::string& path) {
		stbi_set_flip_vertically_on_load(true);
		int width, height, channels;
		stbi_uc* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
		HE_CORE_ASSERT(data, "Failed to load image data!");
		m_Width = width;
		m_Height = height;

		GLenum internalFormat = 0, format = 0;
		switch (channels) {
			case 3: {
				internalFormat = GL_RGB8; 
				format = GL_RGB;
				break;
			}
			case 4: {
				internalFormat = GL_RGBA8; 
				format = GL_RGBA;
				break;
			}
		}

		HE_CORE_ASSERT(internalFormat & format, "Image format not supported!");

		glCreateTextures(GL_TEXTURE_2D, 1, &m_RenderID);
		glTextureStorage2D(m_RenderID, 1, internalFormat, m_Width, m_Height);

		glTextureParameteri(m_RenderID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(m_RenderID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		glTextureSubImage2D(m_RenderID, 0, 0, 0, m_Width, m_Height, format, GL_UNSIGNED_BYTE, data);

		stbi_image_free(data);
		m_Path = path;
	}

	OpenGLTexture2D::OpenGLTexture2D(Ref<TextureResource> textureResource)
		: m_TextureResource(std::move(textureResource)) {
		HE_CORE_ASSERT(m_TextureResource, "OpenGLTexture2D requires a texture resource");
		m_Path = m_TextureResource->GetDesc().SourcePath;
	}

	OpenGLTexture2D::~OpenGLTexture2D() {
		if (m_TextureResource) {
			return;
		}

		glDeleteTextures(1, &m_RenderID);
	}

	uint32_t OpenGLTexture2D::GetRenderID() const {
		if (m_TextureResource) {
			return m_TextureResource->GetRenderID();
		}

		return m_RenderID;
	}

	uint32_t OpenGLTexture2D::GetWidth() const {
		if (m_TextureResource) {
			return m_TextureResource->GetWidth();
		}

		return m_Width;
	}

	uint32_t OpenGLTexture2D::GetHeight() const {
		if (m_TextureResource) {
			return m_TextureResource->GetHeight();
		}

		return m_Height;
	}

	void OpenGLTexture2D::Bind(uint32_t slot) {
		if (m_TextureResource) {
			static_cast<OpenGLTextureResource&>(*m_TextureResource).BindForCommandList(slot);
			return;
		}

		glBindTextureUnit(slot, m_RenderID);
	}
}
