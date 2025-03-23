#include "enginepch.h"
#include "Renderer.h"
#include "VertexBuffer.h"
#include "HuaEngine/Platform/OpenGL/OpenGLVertexBuffer.h"

namespace HE {
	VertexBuffer* VertexBuffer::Create(float* vertices, uint32_t size)
	{
		switch (RendererAPI::GetAPI()) {
		case RendererAPI::API::None: {
				HE_CORE_ASSERT(false, "RenderAPI::None is not supported!");
				return nullptr;
			}
			case RendererAPI::API::OpenGL: {
				return new OpenGLVertexBuffer(vertices, size);
			}
		}
		HE_CORE_ASSERT(false, "Create vertex buffer failed!");
		return nullptr;
	}
}