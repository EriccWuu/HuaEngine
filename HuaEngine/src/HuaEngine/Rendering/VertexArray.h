#pragma once

#include <memory>
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "HuaEngine/Rendering/RHI/VertexBufferView.h"

namespace HE::Rendering {
	class VertexArray {
	public:
		virtual ~VertexArray() = default;
		virtual void AddVertexBuffer(const Ref<VertexBuffer>& vertexBuffer) = 0;
		virtual void SetIndexBuffer(const Ref<IndexBuffer>& indexBuffer) = 0;
		virtual const std::vector<Ref<VertexBuffer>> GetVertexBuffers() const = 0;
		virtual const Ref<IndexBuffer> GetIndexBuffer() const = 0;
		virtual Ref<VertexBufferView> GetVertexBufferView() const = 0;

		static Ref<VertexArray> Create();
	};
}
