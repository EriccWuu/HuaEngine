#pragma once

#include "HuaEngine/Rendering/RenderPipeline/PassGraph.h"
#include "HuaEngine/Rendering/RenderPipeline/RenderPipeline.h"

namespace HE::Rendering {
	class ForwardOpaquePass {
	public:
		void Execute(RenderPassContext& context);
	};

	class ForwardRenderPipeline final : public RenderPipeline {
	public:
		RenderResult Render(
			const RenderView& view,
			const std::vector<RenderItem>& renderItems,
			const RenderResourceResolver& resourceResolver) override;

	private:
		void BuildGraph();
		bool EnsureGraphCompiled(RenderResult& result);
		void CopyGraphStateToResult(RenderResult& result) const;

	private:
		PassGraph m_Graph;
		ForwardOpaquePass m_OpaquePass;
	};
}
