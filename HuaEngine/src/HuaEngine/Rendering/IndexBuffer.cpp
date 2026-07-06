#include "enginepch.h"
#include "IndexBuffer.h"
#include "HuaEngine/Rendering/RHI/RenderHardwareInterface.h"
#include "Platform/OpenGL/OpenGLIndexBuffer.h"

namespace HE::Rendering {
	Ref<IndexBuffer> IndexBuffer::Create(uint32_t* indices, uint32_t count)
	{
		auto gpuBuffer = RenderHardwareInterface::GetDevice().CreateIndexBuffer(indices, count);
		HE_CORE_ASSERT(gpuBuffer, "Expected RenderDevice to create index GPU buffer");
		return CreateRef<OpenGLIndexBuffer>(gpuBuffer, count);
	}
}
