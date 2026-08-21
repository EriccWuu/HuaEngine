#pragma once

#include "HuaEngine/Rendering/RenderPipeline/RenderGraphBuilder.h"

namespace HE::Rendering {
	class PostProcessPass final : public RenderGraphPass {
	public:
		[[nodiscard]] const char* GetName() const override { return "PostProcess"; }
		[[nodiscard]] RenderGraphPassType GetType() const override { return RenderGraphPassType::Graphics; }
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
}
