#pragma once

#include "HuaEngine/Rendering/RenderGraph/RenderGraphBuilder.h"
#include "EditorGridPass.h"

namespace HE::Rendering {
	class ForwardOpaquePass final : public RenderGraphPass {
	public:
		[[nodiscard]] const char* GetName() const override { return "ForwardOpaque"; }
		[[nodiscard]] RenderGraphPassType GetType() const override { return RenderGraphPassType::Graphics; }
		void Configure(
			RenderGraphResourceHandle sceneColor,
			RenderGraphResourceHandle sceneDepth,
			bool writeDepth,
			const glm::vec4& clearColor,
			bool clearColorBuffer,
			bool drawEditorGrid);
		void Setup(RenderGraphPassBuilder& builder) override;
		void Execute(RenderPassContext& context) override;

	private:
		RenderGraphResourceHandle m_SceneColor;
		RenderGraphResourceHandle m_SceneDepth;
		bool m_WriteDepth = false;
		glm::vec4 m_ClearColor = glm::vec4(0.0f);
		bool m_ClearColorBuffer = true;
		bool m_DrawEditorGrid = false;
		EditorGridPass m_EditorGridPass;
	};
}
