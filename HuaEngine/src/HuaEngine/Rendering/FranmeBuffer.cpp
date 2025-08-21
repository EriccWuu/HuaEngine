#include "enginepch.h"
#include "FrameBuffer.h"
#include "RendererAPI.h"
#include "Platform/OpenGL/OpenGLFrameBuffer.h"

namespace HE {
	Ref<FrameBuffer> FrameBuffer::Create(const FrameBufferSpecification& specification)
	{
		switch (RendererAPI::GetAPI()) {
		case RendererAPI::API::None: {
			HE_CORE_ASSERT(false, "RenderAPI::None is not supported!");
			return nullptr;
		}
		case RendererAPI::API::OpenGL: {
			return std::make_shared<OpenGLFrameBuffer>(specification);
		}
		}
		HE_CORE_ASSERT(false, "Create frame buffer failed!");
		return nullptr;
	}
}