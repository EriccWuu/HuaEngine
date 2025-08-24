#include "enginepch.h"
#include "Renderer.h"
#include "VertexBuffer.h"
#include "Platform/OpenGL/OpenGLVertexBuffer.h"

namespace HE::Rendering {
	Ref<VertexBuffer> VertexBuffer::Create(float* vertices, uint32_t size)
	{
		switch (RendererAPI::GetAPI()) {
		case RendererAPI::API::None: {
				HE_CORE_ASSERT(false, "RenderAPI::None is not supported!");
				return nullptr;
			}
			case RendererAPI::API::OpenGL: {
				return std::make_shared<OpenGLVertexBuffer>(vertices, size);
			}
		}
		HE_CORE_ASSERT(false, "Create vertex buffer failed!");
		return nullptr;
	}
}