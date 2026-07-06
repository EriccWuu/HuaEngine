#include "enginepch.h"
#include "FrameBuffer.h"
#include "HuaEngine/Rendering/RHI/RenderHardwareInterface.h"
#include "Platform/OpenGL/OpenGLFrameBuffer.h"

namespace HE::Rendering {
	Ref<FrameBuffer> FrameBuffer::Create(const FrameBufferSpecification& specification)
	{
		auto renderTarget = RenderHardwareInterface::GetDevice().CreateRenderTarget({ .Specification = specification });
		HE_CORE_ASSERT(renderTarget, "FrameBuffer::Create failed to create render target");
		return CreateRef<OpenGLFrameBuffer>(renderTarget);
	}
}
