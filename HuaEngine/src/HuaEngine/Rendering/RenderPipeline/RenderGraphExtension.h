#pragma once

#include "HuaEngine/Rendering/RenderGraph/RenderGraphBuilder.h"
#include "HuaEngine/Rendering/RenderPipeline/RenderTypes.h"

namespace HE::Rendering {
	struct ForwardSceneResources {
		RenderGraphResourceHandle Color;
		RenderGraphResourceHandle Depth;
	};

	class RenderGraphExtension {
	public:
		virtual ~RenderGraphExtension() = default;

		[[nodiscard]] virtual bool RequiresSceneDepth() const { return false; }
		virtual void AddBeforeOpaquePasses(
			RenderGraphBuilder& graph,
			const ForwardSceneResources& resources,
			const RenderView& view) = 0;
		virtual void AddAfterOpaquePasses(
			RenderGraphBuilder& graph,
			const ForwardSceneResources& resources,
			const RenderView& view) {}
	};
}
