#pragma once

#include "HuaEngine/Rendering/RenderPipeline/GraphPasses/BeginRendererPass.h"
#include "HuaEngine/Rendering/RenderPipeline/GraphPasses/EndRendererPass.h"
#include "HuaEngine/Rendering/RenderPipeline/GraphPasses/ForwardOpaquePass.h"
#include "HuaEngine/Rendering/RenderPipeline/GraphPasses/PostProcessPass.h"
#include "HuaEngine/Rendering/RenderGraph/RenderGraph.h"
#include "HuaEngine/Rendering/RenderPipeline/RenderPipeline.h"
#include "HuaEngine/Rendering/RHI/ResourceStateTracker.h"
#include "HuaEngine/Rendering/RHI/CommandSubmission.h"

namespace HE::Rendering {
	class ForwardRenderPipeline final : public RenderPipeline {
	public:
		RenderResult Render(
			const RenderView& view,
			const std::vector<RenderItem>& renderItems,
			const RenderResourceResolver& resourceResolver) override;

	private:
		void BuildGraph(const RenderView& view);
		bool EnsureGraphCompiled(const RenderView& view, RenderResult& result);
		void CopyGraphStateToResult(RenderResult& result) const;

	private:
		RenderGraph m_Graph;
		ResourceStateTracker m_ResourceStates;
		DeferredReleaseQueue m_DeferredReleaseQueue;
		BeginRendererPass m_BeginRendererPass;
		ForwardOpaquePass m_OpaquePass;
		PostProcessPass m_PostProcessPass;
		EndRendererPass m_EndRendererPass;
	};
}
