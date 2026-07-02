#pragma once

#include <vector>

#include "HuaEngine/Rendering/RenderPipeline/RenderTypes.h"

namespace HE::Rendering {
	class RenderPipeline {
	public:
		virtual ~RenderPipeline();
		virtual RenderResult Render(const RenderView& view, const std::vector<RenderItem>& renderItems);
	};
}
