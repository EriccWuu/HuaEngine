#pragma once

#include "HuaEngine/Rendering/RHI/RenderDevice.h"

namespace HE::Rendering {
	class RenderHardwareInterface {
	public:
		static void Init();
		static RenderDevice& GetDevice();
	};
}
