#include "enginepch.h"
#include "RendererAPI.h"
#include "HuaEngine/Platform/OpenGL/OpenGLRendererAPI.h"

namespace HE {
	RendererAPI::API RendererAPI::m_API = RendererAPI::API::OpenGL;

	RendererAPI* RendererAPI::Create()
	{
		switch (m_API) {
		case API::None:
			HE_CORE_ASSERT(false, "API None is not supported!");
			return nullptr;
		case API::OpenGL:
			return new OpenGLRendererAPI();
		}

		HE_CORE_ASSERT(false, "No matching graphic API!");
		return nullptr;
	}
}