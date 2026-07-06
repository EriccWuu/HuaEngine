#include "enginepch.h"
#include "VertexBuffer.h"
#include "HuaEngine/Rendering/RHI/RenderHardwareInterface.h"
#include "Platform/OpenGL/OpenGLVertexBuffer.h"

namespace HE::Rendering {
	Ref<VertexBuffer> VertexBuffer::Create(float* vertices, uint32_t size)
	{
		auto gpuBuffer = RenderHardwareInterface::GetDevice().CreateVertexBuffer(vertices, size);
		HE_CORE_ASSERT(gpuBuffer, "Expected RenderDevice to create vertex GPU buffer");
		return CreateRef<OpenGLVertexBuffer>(gpuBuffer);
	}
}
