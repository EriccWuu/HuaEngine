#pragma once

#include "HuaEngine/Rendering/RHI/RenderDevice.h"

namespace HE::Rendering {
	class RenderHardwareInterface {
	public:
		static void Init();
		static void Init(const RenderDeviceDesc& desc);
		[[nodiscard]] static bool IsInitialized();
		static Scope<RenderDevice> CreateRenderDevice(const RenderDeviceDesc& desc);
		static RenderDevice& GetDevice();
	};
}
