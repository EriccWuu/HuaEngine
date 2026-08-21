#pragma once

#include <vector>

#include "HuaEngine/Rendering/RenderPipeline/RenderTypes.h"

namespace HE::Rendering {
	class RenderResourceResolver;
	class RenderGraphExtension;

	class RenderPipeline {
	public:
		virtual ~RenderPipeline();
		virtual RenderResult Render(
			const RenderView& view,
			const std::vector<RenderItem>& renderItems,
			const RenderResourceResolver& resourceResolver,
			RenderGraphExtension* extension = nullptr) = 0;
	};
}
