#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include "HuaEngine/ECS/EntityId.h"
#include "HuaEngine/Rendering/RenderPipeline/RenderGraphExtension.h"

namespace HE::Rendering {
	class RenderTarget;
}

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
			Rendering::RenderGraphResourceHandle objectIdDepth);
		void Setup(Rendering::RenderGraphPassBuilder& builder) override;
		void Execute(Rendering::RenderPassContext& context) override;
		void ResetPicks() noexcept { m_Picks.clear(); m_FirstToken = m_NextToken; }
		[[nodiscard]] EntityId ResolveObjectId(uint32_t token, uint64_t worldId) const noexcept;

	private:
		struct PickIdentity { EntityId Entity; uint64_t WorldId; };
		Rendering::RenderGraphResourceHandle m_ObjectId;
		Rendering::RenderGraphResourceHandle m_ObjectIdDepth;
		std::vector<PickIdentity> m_Picks;
		uint64_t m_FirstToken = 1;
		uint64_t m_NextToken = 1;
	};

	class EditorSceneRenderExtension final : public Rendering::RenderGraphExtension {
	public:
		void SetObjectIdTarget(Ref<Rendering::RenderTarget> target) { m_ObjectIdTarget = std::move(target); }
		[[nodiscard]] bool RequiresSceneDepth() const override { return true; }
		void AddBeforeOpaquePasses(
			Rendering::RenderGraphBuilder& graph,
			const Rendering::ForwardSceneResources& resources,
			const Rendering::RenderView& view) override;
		void AddAfterOpaquePasses(
			Rendering::RenderGraphBuilder& graph,
			const Rendering::ForwardSceneResources& resources,
			const Rendering::RenderView& view) override;
		[[nodiscard]] EntityId ResolveObjectId(uint32_t token, uint64_t worldId) const noexcept {
			return m_EditorObjectIdPass.ResolveObjectId(token, worldId);
		}

	private:
		Ref<Rendering::RenderTarget> m_ObjectIdTarget;
		EditorGridPass m_EditorGridPass;
		EditorObjectIdPass m_EditorObjectIdPass;
	};
}
