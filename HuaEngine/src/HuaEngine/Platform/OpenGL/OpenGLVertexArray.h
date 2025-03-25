#pragma once

#include "HuaEngine/Renderer/VertexArray.h"

namespace HE {
	class OpenGLVertexArray : public VertexArray {
	public:
		OpenGLVertexArray();
		~OpenGLVertexArray();
		virtual void Bind() override;
		virtual void Unbind() override;
		virtual void AddVertexBuffer(const Ref<VertexBuffer> vertexBuffer) override;
		virtual void SetIndexBuffer(const Ref<IndexBuffer> indexBuffer) override;
		virtual const std::vector<Ref<VertexBuffer>> GetVertexBuffers() const override;
		virtual const Ref<IndexBuffer> GetIndexBuffer() const override;

	private:
		unsigned int m_RenderId;
		std::vector<Ref<VertexBuffer>> m_VertexBuffers;
		Ref<IndexBuffer> m_IndexBuffer;
	};
}