#include "enginepch.h"
#include "OpenGLVertexArray.h"
#include "OpenGLIndexBuffer.h"
#include "OpenGLVertexBuffer.h"
#include "HuaEngine/Rendering/RHI/RenderHardwareInterface.h"
#include "glad/glad.h"

#include <cstdint>

namespace HE::Rendering {
	static uint32_t GetOpenGLType(ShaderDataType dataType) {
		switch (dataType) {
		case ShaderDataType::Float:  return GL_FLOAT;
		case ShaderDataType::Float2: return GL_FLOAT;
		case ShaderDataType::Float3: return GL_FLOAT;
		case ShaderDataType::Float4: return GL_FLOAT;
		case ShaderDataType::Int:    return GL_INT;
		case ShaderDataType::Int2:   return GL_INT;
		case ShaderDataType::Int3:   return GL_INT;
		case ShaderDataType::Int4:   return GL_INT;
		case ShaderDataType::Mat3:   return GL_FLOAT;
		case ShaderDataType::Mat4:   return GL_FLOAT;
		case ShaderDataType::Bool:   return GL_BOOL;
		}
		HE_CORE_ASSERT(false, "Unkonwn ShaderDataType");
		return 0;
	}

	OpenGLVertexArray::OpenGLVertexArray() {
		glGenVertexArrays(1, &m_RenderID);
	}

	OpenGLVertexArray::~OpenGLVertexArray() {
		glDeleteVertexArrays(1, &m_RenderID);
	}

	void OpenGLVertexArray::Bind() {
		if (m_VertexBufferView) {
			m_VertexBufferView->Bind();
			return;
		}

		glBindVertexArray(m_RenderID);
	}

	void OpenGLVertexArray::Unbind() {
		if (m_VertexBufferView) {
			m_VertexBufferView->Unbind();
			return;
		}

		glBindVertexArray(0);
	}

	void OpenGLVertexArray::AddVertexBuffer(const std::shared_ptr<VertexBuffer>& vertexBuffer) {
		HE_CORE_ASSERT(vertexBuffer->GetLayout().GetElements().size(), "Vertex buffer layout not set!");
		glBindVertexArray(m_RenderID);
		vertexBuffer->Bind();
		
		uint32_t idx = 0;
		auto& layout = vertexBuffer->GetLayout();
		for (auto& element : layout) {
			glEnableVertexAttribArray(idx);
			glVertexAttribPointer(idx,
				ShaderDataTypeByteCount(element.Type),
				GetOpenGLType(element.Type),
				element.Normalized ? GL_TRUE : GL_FALSE,
				layout.GetStride(),
				reinterpret_cast<const void*>(static_cast<std::uintptr_t>(element.Offset)));
			++idx;
		}

		m_VertexBuffers.push_back(vertexBuffer);
		TryCreateVertexBufferView();
	}

	void OpenGLVertexArray::SetIndexBuffer(const std::shared_ptr<IndexBuffer>& indexBuffer) {
		glBindVertexArray(m_RenderID);
		indexBuffer->Bind();

		m_IndexBuffer = indexBuffer;
		TryCreateVertexBufferView();
	}

	const std::vector<std::shared_ptr<VertexBuffer>> OpenGLVertexArray::GetVertexBuffers() const {
		return m_VertexBuffers;
	}

	const std::shared_ptr<IndexBuffer> OpenGLVertexArray::GetIndexBuffer() const {
		return m_IndexBuffer;
	}

	void OpenGLVertexArray::TryCreateVertexBufferView() {
		m_VertexBufferView = nullptr;

		if (m_VertexBuffers.empty() || !m_IndexBuffer) {
			return;
		}

		const auto& firstVertexBuffer = m_VertexBuffers.front();
		if (!firstVertexBuffer || firstVertexBuffer->GetLayout().GetElements().empty() || m_IndexBuffer->GetCount() == 0) {
			return;
		}

		auto openGLVertexBuffer = std::dynamic_pointer_cast<OpenGLVertexBuffer>(firstVertexBuffer);
		auto openGLIndexBuffer = std::dynamic_pointer_cast<OpenGLIndexBuffer>(m_IndexBuffer);
		if (!openGLVertexBuffer || !openGLIndexBuffer || !openGLVertexBuffer->GetGpuBuffer() || !openGLIndexBuffer->GetGpuBuffer()) {
			return;
		}

		m_VertexBufferView = RenderHardwareInterface::GetDevice().CreateVertexBufferView({
			.VertexBuffer = openGLVertexBuffer->GetGpuBuffer(),
			.IndexBuffer = openGLIndexBuffer->GetGpuBuffer(),
			.Layout = firstVertexBuffer->GetLayout(),
			.IndexFormatValue = IndexFormat::UInt32,
			.IndexCount = m_IndexBuffer->GetCount()
		});
	}
}
