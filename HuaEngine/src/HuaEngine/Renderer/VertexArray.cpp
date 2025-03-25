#include "enginepch.h"
#include "VertexArray.h"
#include "Renderer.h"
#include "HuaEngine/Platform/OpenGL/OpenGLVertexArray.h"

namespace HE {
	Ref<VertexArray> VertexArray::Create()
	{
		switch (RendererAPI::GetAPI()) {
		case RendererAPI::API::None: {
			HE_CORE_ASSERT(false, "RenderAPI::None is not supported!");
			return nullptr;
		}
		case RendererAPI::API::OpenGL: {
			return std::make_shared<OpenGLVertexArray>();
		}
		}
		HE_CORE_ASSERT(false, "Create vertex array failed!");
		return nullptr;
	}
}