#pragma once

#include "HuaEngine/Rendering/RHI/GpuBuffer.h"
#include "HuaEngine/Rendering/VertexBuffer.h"

namespace HE::Rendering {
	class OpenGLVertexBuffer : public VertexBuffer {
	public:
		OpenGLVertexBuffer(float* vertices, uint32_t size);
		explicit OpenGLVertexBuffer(Ref<GpuBuffer> gpuBuffer);
		~OpenGLVertexBuffer();
		virtual void Bind() const override;
		virtual void Unbind() const override;
		virtual void SetLayout(BufferLayout& layout) override;
		virtual const BufferLayout& GetLayout() const override;
		const Ref<GpuBuffer>& GetGpuBuffer() const;

	private:
		unsigned int m_RenderID = 0;
		Ref<GpuBuffer> m_GpuBuffer;
		BufferLayout m_BufferLayout;
	};
}
