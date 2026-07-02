#pragma once

#include <vector>

#include "HuaEngine/Rendering/RenderPipeline/RenderTypes.h"

namespace HE {
	class World;
}

namespace HE::Rendering {
	class SceneRenderExtractor {
	public:
		[[nodiscard]] std::vector<RenderItem> Extract(World& world) const;
	};
}
