#include "enginepch.h"
#include "RenderHardwareInterface.h"

#include "Platform/OpenGL/RHI/OpenGLRenderDevice.h"

namespace HE::Rendering {
	namespace {
		Scope<RenderDevice> s_Device;
	}

	void RenderHardwareInterface::Init() {
		Init(RenderDeviceDesc{});
	}

	void RenderHardwareInterface::Init(const RenderDeviceDesc& desc) {
		if (!s_Device) {
			s_Device = CreateRenderDevice(desc);
		}
	}

	bool RenderHardwareInterface::IsInitialized() {
		return static_cast<bool>(s_Device);
	}

	Scope<RenderDevice> RenderHardwareInterface::CreateRenderDevice(const RenderDeviceDesc& desc) {
		switch (desc.Backend) {
			case RenderBackendType::OpenGL:
				return CreateScope<OpenGLRenderDevice>(desc);
			case RenderBackendType::Vulkan:
			case RenderBackendType::D3D12:
			case RenderBackendType::Metal:
			case RenderBackendType::Null:
				HE_CORE_ERROR("Requested render backend is not implemented");
				return nullptr;
		}

		HE_CORE_ERROR("Unknown render backend");
		return nullptr;
	}

	RenderDevice& RenderHardwareInterface::GetDevice() {
		Init();
		return *s_Device;
	}
}
