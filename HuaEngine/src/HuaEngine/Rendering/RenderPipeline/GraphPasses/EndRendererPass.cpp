#include "enginepch.h"
#include "EndRendererPass.h"

#include "HuaEngine/Rendering/RHI/CommandList.h"

namespace HE::Rendering {
	void EndRendererPass::Execute(RenderPassContext& context) {
		if (!context.Commands || !context.Stats) {
			return;
		}

		++context.Stats->PassCount;
		context.Commands->EndFrame();
	}

}
