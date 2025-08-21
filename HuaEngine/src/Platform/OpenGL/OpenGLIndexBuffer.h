#pragma once

#include "HuaEngine/Rendering/IndexBuffer.h"

namespace HE {
	class OpenGLIndexBuffer : public IndexBuffer {
	public:
		OpenGLIndexBuffer(uint32_t* indices, uint32_t count);
		~OpenGLIndexBuffer();
		virtual void Bind() const override;
		virtual void Unbind() const override;
		virtual uint32_t GetCount() const override;
	private:
		uint32_t m_RenderID, m_Count;
	};
}