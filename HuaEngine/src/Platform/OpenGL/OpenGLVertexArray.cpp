#include "enginepch.h"
#include "OpenGLVertexArray.h"
#include "glad/glad.h"

namespace HE {
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
		glBindVertexArray(m_RenderID);
	}

	void OpenGLVertexArray::Unbind() {
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
				(const void*)element.Offset);
			++idx;
		}

		m_VertexBuffers.push_back(vertexBuffer);
	}

	void OpenGLVertexArray::SetIndexBuffer(const std::shared_ptr<IndexBuffer>& indexBuffer) {
		glBindVertexArray(m_RenderID);
		indexBuffer->Bind();

		m_IndexBuffer = indexBuffer;
	}

	const std::vector<std::shared_ptr<VertexBuffer>> OpenGLVertexArray::GetVertexBuffers() const {
		return m_VertexBuffers;
	}

	const std::shared_ptr<IndexBuffer> OpenGLVertexArray::GetIndexBuffer() const {
		return m_IndexBuffer;
	}
}