#include "enginepch.h"
#include "RenderHardwareInterface.h"

#include "Platform/OpenGL/RHI/OpenGLRenderDevice.h"

namespace HE::Rendering {
	namespace {
		Scope<RenderDevice> s_Device;
	}

	void RenderHardwareInterface::Init() {
		if (!s_Device) {
			s_Device = CreateScope<OpenGLRenderDevice>();
		}
	}

	RenderDevice& RenderHardwareInterface::GetDevice() {
		Init();
		return *s_Device;
	}
}
