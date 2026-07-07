#pragma once

#include "HuaEngine/Rendering/VertexArray.h"
#include "HuaEngine/Rendering/RHI/VertexBufferView.h"

namespace HE::Rendering {
	class OpenGLVertexArray : public VertexArray {
	public:
		OpenGLVertexArray();
		~OpenGLVertexArray();
		virtual void AddVertexBuffer(const Ref<VertexBuffer>& vertexBuffer) override;
		virtual void SetIndexBuffer(const Ref<IndexBuffer>& indexBuffer) override;
		virtual const std::vector<Ref<VertexBuffer>> GetVertexBuffers() const override;
		virtual const Ref<IndexBuffer> GetIndexBuffer() const override;
		Ref<VertexBufferView> GetVertexBufferView() const override { return m_VertexBufferView; }
		void BindForCommandList();
		void UnbindForCommandList();

	private:
		void TryCreateVertexBufferView();

		unsigned int m_RenderID;
		std::vector<Ref<VertexBuffer>> m_VertexBuffers;
		Ref<IndexBuffer> m_IndexBuffer;
		Ref<VertexBufferView> m_VertexBufferView;
	};
}
