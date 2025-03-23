#pragma once

#include "HuaEngine/Renderer/VertexArray.h"

namespace HE {
	class OpenGLVertexArray : public VertexArray {
	public:
		OpenGLVertexArray();
		~OpenGLVertexArray();
		virtual void Bind() override;
		virtual void Unbind() override;
		virtual void AddVertexBuffer(const std::shared_ptr<VertexBuffer> vertexBuffer) override;
		virtual void SetIndexBuffer(const std::shared_ptr<IndexBuffer> indexBuffer) override;
		virtual const std::vector<std::shared_ptr<VertexBuffer>> GetVertexBuffers() const override;
		virtual const std::shared_ptr<IndexBuffer> GetIndexBuffer() const override;

	private:
		unsigned int m_RenderId;
		std::vector<std::shared_ptr<VertexBuffer>> m_VertexBuffers;
		std::shared_ptr<IndexBuffer> m_IndexBuffer;
	};
}