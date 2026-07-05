#pragma once

#include "HuaEngine/Rendering/RenderPipeline/PassGraph.h"
#include "HuaEngine/Rendering/RenderPipeline/RenderPipeline.h"

namespace HE::Rendering {
	class BindTargetPass {
	public:
		void Execute(RenderPassContext& context);
	};

	class ClearTargetPass {
	public:
		void Execute(RenderPassContext& context);
	};

	class BeginRendererPass {
	public:
		void Execute(RenderPassContext& context);
	};

	class ForwardOpaquePass {
	public:
		void Execute(RenderPassContext& context);
	};

	class EndRendererPass {
	public:
		void Execute(RenderPassContext& context);
	};

	class UnbindTargetPass {
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
		BindTargetPass m_BindTargetPass;
		ClearTargetPass m_ClearTargetPass;
		BeginRendererPass m_BeginRendererPass;
		ForwardOpaquePass m_OpaquePass;
		EndRendererPass m_EndRendererPass;
		UnbindTargetPass m_UnbindTargetPass;
	};
}
