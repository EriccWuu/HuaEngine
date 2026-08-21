#pragma once

#include "HuaEngine/Rendering/RenderPipeline/RenderTypes.h"

namespace HE::Rendering {
	class EditorGridPass final {
	public:
		void Execute(RenderPassContext& context) const;
	};
}
