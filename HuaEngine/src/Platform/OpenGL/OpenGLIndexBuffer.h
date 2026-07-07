#pragma once

#include "HuaEngine/Rendering/IndexBuffer.h"
#include "HuaEngine/Rendering/RHI/GpuBuffer.h"

namespace HE::Rendering {
	class OpenGLIndexBuffer : public IndexBuffer {
	public:
		OpenGLIndexBuffer(uint32_t* indices, uint32_t count);
		OpenGLIndexBuffer(Ref<GpuBuffer> gpuBuffer, uint32_t count);
		~OpenGLIndexBuffer();
		virtual uint32_t GetCount() const override;
		void BindForVertexArrayBuild() const;
		void BindForMeshDataRead() const;
		const Ref<GpuBuffer>& GetGpuBuffer() const;
	private:
		uint32_t m_RenderID = 0;
		uint32_t m_Count = 0;
		Ref<GpuBuffer> m_GpuBuffer;
	};
}
