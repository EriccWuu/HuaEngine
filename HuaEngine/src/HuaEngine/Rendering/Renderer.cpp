#include "enginepch.h"
#include "Renderer.h"

#include "HuaEngine/Rendering/RHI/RenderHardwareInterface.h"

namespace HE::Rendering {
	void Renderer::Init() {
		RenderHardwareInterface::Init();
	}
}
