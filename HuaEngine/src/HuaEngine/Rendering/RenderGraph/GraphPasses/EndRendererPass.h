#pragma once

#include "HuaEngine/Rendering/RenderPipeline/RenderTypes.h"

namespace HE::Rendering {
	class EndRendererPass {
	public:
		void Execute(RenderPassContext& context);
	};
}
