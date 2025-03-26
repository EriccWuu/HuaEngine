#include "enginepch.h"
#include "OpenGLVertexBuffer.h"
#include "glad/glad.h"

namespace HE {
	OpenGLVertexBuffer::OpenGLVertexBuffer(float* vertices, uint32_t size) {
		glGenBuffers(1, &m_RenderID);
		glBindBuffer(GL_ARRAY_BUFFER, m_RenderID);
		glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_STATIC_DRAW);
	}

	OpenGLVertexBuffer::~OpenGLVertexBuffer() {
		glDeleteBuffers(1, &m_RenderID);
	}

	void OpenGLVertexBuffer::Bind() const {
		glBindBuffer(GL_ARRAY_BUFFER, m_RenderID);
	}

	void OpenGLVertexBuffer::Unbind() const {
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	void OpenGLVertexBuffer::SetLayout(BufferLayout& layout)
	{
		m_BufferLayout = layout;
	}

	const BufferLayout& OpenGLVertexBuffer::GetLayout() const
	{
		return m_BufferLayout;
	}
}

