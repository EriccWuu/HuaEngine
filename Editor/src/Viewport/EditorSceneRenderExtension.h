#pragma once

#include "HuaEngine/Rendering/RenderPipeline/RenderGraphExtension.h"

namespace HE::Editor {
	class EditorGridPass final : public Rendering::RenderGraphPass {
	public:
		[[nodiscard]] const char* GetName() const override { return "EditorGrid"; }
		[[nodiscard]] Rendering::RenderGraphPassType GetType() const override { return Rendering::RenderGraphPassType::Graphics; }
		void Configure(
			Rendering::RenderGraphResourceHandle sceneColor,
			Rendering::RenderGraphResourceHandle sceneDepth,
			const glm::vec4& clearColor);
		void Setup(Rendering::RenderGraphPassBuilder& builder) override;
		void Execute(Rendering::RenderPassContext& context) override;

	private:
		Rendering::RenderGraphResourceHandle m_SceneColor;
		Rendering::RenderGraphResourceHandle m_SceneDepth;
		glm::vec4 m_ClearColor = glm::vec4(0.0f);
	};

	class EditorObjectIdPass final : public Rendering::RenderGraphPass {
	public:
		[[nodiscard]] const char* GetName() const override { return "EditorObjectId"; }
		[[nodiscard]] Rendering::RenderGraphPassType GetType() const override { return Rendering::RenderGraphPassType::Graphics; }
		void Configure(
			Rendering::RenderGraphResourceHandle objectId,
			Rendering::RenderGraphResourceHandle sceneDepth);
		void Setup(Rendering::RenderGraphPassBuilder& builder) override;
		void Execute(Rendering::RenderPassContext& context) override;

	private:
		Rendering::RenderGraphResourceHandle m_ObjectId;
		Rendering::RenderGraphResourceHandle m_SceneDepth;
	};

	class EditorSceneRenderExtension final : public Rendering::RenderGraphExtension {
	public:
		[[nodiscard]] bool RequiresSceneDepth() const override { return true; }
		void AddBeforeOpaquePasses(
			Rendering::RenderGraphBuilder& graph,
			const Rendering::ForwardSceneResources& resources,
			const Rendering::RenderView& view) override;
		void AddAfterOpaquePasses(
			Rendering::RenderGraphBuilder& graph,
			const Rendering::ForwardSceneResources& resources,
			const Rendering::RenderView& view) override;

	private:
		EditorGridPass m_EditorGridPass;
		EditorObjectIdPass m_EditorObjectIdPass;
	};
}
