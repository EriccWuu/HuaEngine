#pragma once

#include <string>
#include "HuaEngine/Rendering/Texture.h"
#include "stb_image.h"

namespace HE {
	class OpenGLTexture2D : public Texture2D {
	public:
		OpenGLTexture2D(const std::string& path);
		~OpenGLTexture2D();
		virtual uint32_t GetRenderID() const override { return m_RenderID; };
		virtual uint32_t GetWidth() const override;
		virtual uint32_t GetHeight() const override;
		virtual void Bind(uint32_t slot = 0) override;

	private:
		std::string m_Path;
		uint32_t m_Width, m_Height;
		uint32_t m_RenderID;
	};
}