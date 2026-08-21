#pragma once

#include "HuaEngine/ECS/System.h"
#include "HuaEngine/Rendering/Renderer.h"
#include "HuaEngine/Rendering/RenderPipeline/RenderPipeline.h"
#include "HuaEngine/Rendering/RenderPipeline/RenderGraphExtension.h"
#include "HuaEngine/Rendering/RenderPipeline/RenderResourceResolver.h"
#include "HuaEngine/Rendering/RHI/RenderTarget.h"
#include "Module/Rendering/SceneRenderExtractor.h"

namespace HE {
	class AssetResolver;

	class RenderSystem : public System {
	public:
		RenderSystem();
		SystemDescriptor Describe() const override;
		void Update(SystemContext& context) override;

		void RenderSingleCamera(
			World& world,
			const Rendering::RenderCamera& camera,
			Rendering::RenderGraphExtension* extension = nullptr);
		[[nodiscard]] const Rendering::RenderResult& GetLastRenderResult() const { return m_LastRenderResult; }
		void SetRenderTarget(const Ref<Rendering::RenderTarget>& renderTarget) { m_RenderTarget = renderTarget; }
		void SetAssetResolver(AssetResolver* resolver) { m_ResourceResolver.SetAssetResolver(resolver); }

	private:
		Ref<Rendering::RenderTarget> m_RenderTarget;
		SceneRenderExtractor m_Extractor;
		Rendering::RenderResourceResolver m_ResourceResolver;
		Scope<Rendering::RenderPipeline> m_RenderPipeline;
		Rendering::RenderResult m_LastRenderResult;
	};
}
