#pragma once

#include <vector>

#include "HuaEngine/Rendering/RenderPipeline/RenderTypes.h"

namespace HE {
	class World;

	class SceneRenderExtractor {
	public:
		[[nodiscard]] std::vector<Rendering::RenderItem> Extract(World& world) const;
	};
}
