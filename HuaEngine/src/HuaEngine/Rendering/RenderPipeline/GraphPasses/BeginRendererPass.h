#pragma once

#include "HuaEngine/Rendering/RenderPipeline/RenderTypes.h"

namespace HE::Rendering {
	class BeginRendererPass {
	public:
		void Execute(RenderPassContext& context);
	};
}
