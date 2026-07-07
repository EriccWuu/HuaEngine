#pragma once

#include <string>
#include "HuaEngine/Rendering/Texture.h"
#include "HuaEngine/Rendering/RHI/TextureResource.h"
#include "stb_image.h"

namespace HE::Rendering {
	class OpenGLTexture2D : public Texture2D {
	public:
		OpenGLTexture2D(const std::string& path);
		explicit OpenGLTexture2D(Ref<TextureResource> textureResource);
		~OpenGLTexture2D();
		virtual uint32_t GetRenderID() const override;
		virtual uint32_t GetWidth() const override;
		virtual uint32_t GetHeight() const override;
		void BindForCommandList(uint32_t slot = 0);
		Ref<TextureResource> GetTextureResource() const override { return m_TextureResource; }

	private:
		uint32_t m_Width = 0, m_Height = 0;
		uint32_t m_RenderID = 0;
		Ref<TextureResource> m_TextureResource;
	};
}
