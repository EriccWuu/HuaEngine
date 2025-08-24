#include "enginepch.h"
#include "RendererAPI.h"
#include "IndexBuffer.h"
#include "Platform/OpenGL/OpenGLIndexBuffer.h"

namespace HE::Rendering {
	Ref<IndexBuffer> IndexBuffer::Create(uint32_t* indices, uint32_t count)
	{
		switch (RendererAPI::GetAPI()) {
		case RendererAPI::API::None: {
				HE_CORE_ASSERT(false, "RenderAPI::None is not supported!");
				return nullptr;
			}
			case RendererAPI::API::OpenGL: {
				return std::make_shared<OpenGLIndexBuffer>(indices, count);
			}
		}
		HE_CORE_ASSERT(false, "Create index buffer failed!");
		return nullptr;
	}
}