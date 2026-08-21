#include "enginepch.h"
#include "BeginRendererPass.h"

#include "HuaEngine/Rendering/RHI/CommandList.h"

namespace HE::Rendering {
	void BeginRendererPass::Execute(RenderPassContext& context) {
		if (!context.View || !context.View->CameraRef || !context.Commands || !context.Stats) {
			return;
		}

		++context.Stats->PassCount;
		context.Commands->BeginFrame();
	}

}
