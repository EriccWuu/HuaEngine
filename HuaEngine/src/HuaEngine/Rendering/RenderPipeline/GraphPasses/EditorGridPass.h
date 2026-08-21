#pragma once

#include "HuaEngine/Rendering/RenderGraph/RenderGraphBuilder.h"

namespace HE::Rendering {
	class EditorGridPass final : public RenderGraphPass {
	public:
		[[nodiscard]] const char* GetName() const override { return "EditorGrid"; }
		[[nodiscard]] RenderGraphPassType GetType() const override { return RenderGraphPassType::Graphics; }
		void Configure(
			RenderGraphResourceHandle sceneColor,
			RenderGraphResourceHandle sceneDepth,
			const glm::vec4& clearColor);
		void Setup(RenderGraphPassBuilder& builder) override;
		void Execute(RenderPassContext& context) override;

	private:
		RenderGraphResourceHandle m_SceneColor;
		RenderGraphResourceHandle m_SceneDepth;
		glm::vec4 m_ClearColor = glm::vec4(0.0f);
	};
}
