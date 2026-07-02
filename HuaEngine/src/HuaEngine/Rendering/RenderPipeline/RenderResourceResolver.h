#pragma once

#include <vector>

#include "HuaEngine/Rendering/RenderPipeline/RenderTypes.h"

namespace HE::Rendering {
	class RenderResourceResolver {
	public:
		bool Resolve(
			const RenderItem& item,
			ResolvedRenderItem& outResolvedItem,
			std::vector<RenderDiagnostic>& diagnostics) const;
	};
}
