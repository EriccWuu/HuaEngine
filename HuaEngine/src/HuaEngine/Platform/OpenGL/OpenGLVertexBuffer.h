#pragma once

#include "HuaEngine/Renderer/VertexBuffer.h"

namespace HE {
	class OpenGLVertexBuffer : public VertexBuffer {
	public:
		OpenGLVertexBuffer(float* vertices, uint32_t size);
		~OpenGLVertexBuffer();
		virtual void Bind() const override;
		virtual void Unbind() const override;
		virtual void SetLayout(BufferLayout& layout) override;
		virtual const BufferLayout& GetLayout() const override;

	private:
		unsigned int m_RenderID;
		BufferLayout m_BufferLayout;
	};
}