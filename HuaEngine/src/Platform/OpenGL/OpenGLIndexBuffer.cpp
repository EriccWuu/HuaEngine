#include "enginepch.h"
#include "OpenGLIndexBuffer.h"
#include "glad/glad.h"
#include "Platform/OpenGL/RHI/OpenGLRenderDevice.h"
#include <utility>

namespace HE::Rendering {
	OpenGLIndexBuffer::OpenGLIndexBuffer(uint32_t* indices, uint32_t count)
	: m_Count(count) {
		glGenBuffers(1, &m_RenderID);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RenderID);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(uint32_t), indices, GL_STATIC_DRAW);
	}

	OpenGLIndexBuffer::OpenGLIndexBuffer(Ref<GpuBuffer> gpuBuffer, uint32_t count)
		: m_Count(count), m_GpuBuffer(std::move(gpuBuffer)) {
		HE_CORE_ASSERT(m_GpuBuffer, "OpenGLIndexBuffer requires a GPU buffer");
	}

	OpenGLIndexBuffer::~OpenGLIndexBuffer() {
		if (m_GpuBuffer) {
			return;
		}

		glDeleteBuffers(1, &m_RenderID);
	}

	void OpenGLIndexBuffer::Bind() const {
		if (m_GpuBuffer) {
			static_cast<OpenGLGpuBuffer&>(*m_GpuBuffer).BindForCommandList();
			return;
		}

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RenderID);
	}

	void OpenGLIndexBuffer::Unbind() const {
		if (m_GpuBuffer) {
			static_cast<OpenGLGpuBuffer&>(*m_GpuBuffer).UnbindForCommandList();
			return;
		}

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}

	uint32_t OpenGLIndexBuffer::GetCount() const {
		return m_Count;
	}

	const Ref<GpuBuffer>& OpenGLIndexBuffer::GetGpuBuffer() const {
		return m_GpuBuffer;
	}
}
