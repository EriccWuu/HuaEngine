#pragma once

#include "HuaEngine/Rendering/RenderPipeline/RenderGraphBuilder.h"
#include "HuaEngine/Rendering/RenderPipeline/RenderPipeline.h"
#include "HuaEngine/Rendering/RHI/ResourceStateTracker.h"
#include "HuaEngine/Rendering/RHI/CommandSubmission.h"

namespace HE::Rendering {
	class BeginRendererPass {
	public:
		void Execute(RenderPassContext& context);
	};

	class ForwardOpaquePass final : public RenderGraphPass {
	public:
		void Configure(
			RenderGraphResourceHandle sceneColor,
			RenderGraphResourceHandle sceneDepth,
			bool writeDepth,
			const glm::vec4& clearColor,
			bool clearColorBuffer);
		void Setup(RenderGraphPassBuilder& builder) override;
		void Execute(RenderPassContext& context) override;

	private:
		RenderGraphResourceHandle m_SceneColor;
		RenderGraphResourceHandle m_SceneDepth;
		bool m_WriteDepth = false;
		glm::vec4 m_ClearColor = glm::vec4(0.0f);
		bool m_ClearColorBuffer = true;
	};

	class PostProcessPass final : public RenderGraphPass {
	public:
		void Configure(
			RenderGraphResourceHandle sceneColor,
			RenderGraphResourceHandle output,
			const glm::vec4& clearColor);
		void Setup(RenderGraphPassBuilder& builder) override;
		void Execute(RenderPassContext& context) override;

	private:
		RenderGraphResourceHandle m_SceneColor;
		RenderGraphResourceHandle m_Output;
		glm::vec4 m_ClearColor = glm::vec4(0.0f);
	};

	class EndRendererPass {
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
		void BuildGraph(const RenderView& view);
		bool EnsureGraphCompiled(const RenderView& view, RenderResult& result);
		void CopyGraphStateToResult(RenderResult& result) const;

	private:
		PassGraph m_Graph;
		ResourceStateTracker m_ResourceStates;
		DeferredReleaseQueue m_DeferredReleaseQueue;
		BeginRendererPass m_BeginRendererPass;
		ForwardOpaquePass m_OpaquePass;
		PostProcessPass m_PostProcessPass;
		EndRendererPass m_EndRendererPass;
	};
}
