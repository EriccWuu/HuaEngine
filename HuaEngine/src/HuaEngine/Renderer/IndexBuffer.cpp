#include "enginepch.h"
#include "Renderer.h"
#include "IndexBuffer.h"
#include "HuaEngine/Platform/OpenGL/OpenGLIndexBuffer.h"

namespace HE {
	IndexBuffer* IndexBuffer::Create(uint32_t* indices, uint32_t count)
	{
		switch (RendererAPI::GetAPI()) {
		case RendererAPI::API::None: {
				HE_CORE_ASSERT(false, "RenderAPI::None is not supported!");
				return nullptr;
			}
			case RendererAPI::API::OpenGL: {
				return new OpenGLIndexBuffer(indices, count);
			}
		}
		HE_CORE_ASSERT(false, "Create index buffer failed!");
		return nullptr;
	}
}