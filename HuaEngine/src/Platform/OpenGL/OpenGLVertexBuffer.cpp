#include "enginepch.h"
#include "OpenGLVertexBuffer.h"
#include "glad/glad.h"
#include <utility>

namespace HE::Rendering {
	OpenGLVertexBuffer::OpenGLVertexBuffer(float* vertices, uint32_t size) {
		glGenBuffers(1, &m_RenderID);
		glBindBuffer(GL_ARRAY_BUFFER, m_RenderID);
		glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_STATIC_DRAW);
	}

	OpenGLVertexBuffer::OpenGLVertexBuffer(Ref<GpuBuffer> gpuBuffer)
		: m_GpuBuffer(std::move(gpuBuffer)) {
		HE_CORE_ASSERT(m_GpuBuffer, "OpenGLVertexBuffer requires a GPU buffer");
	}

	OpenGLVertexBuffer::~OpenGLVertexBuffer() {
		if (m_GpuBuffer) {
			return;
		}

		glDeleteBuffers(1, &m_RenderID);
	}

	void OpenGLVertexBuffer::Bind() const {
		if (m_GpuBuffer) {
			m_GpuBuffer->Bind();
			return;
		}

		glBindBuffer(GL_ARRAY_BUFFER, m_RenderID);
	}

	void OpenGLVertexBuffer::Unbind() const {
		if (m_GpuBuffer) {
			m_GpuBuffer->Unbind();
			return;
		}

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

	const Ref<GpuBuffer>& OpenGLVertexBuffer::GetGpuBuffer() const
	{
		return m_GpuBuffer;
	}
}
