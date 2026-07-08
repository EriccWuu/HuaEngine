#pragma once

#include "HuaEngine/ECS/System.h"
#include "HuaEngine/Scene/Scene.h"
#include "HuaEngine/Rendering/Renderer.h"
#include "HuaEngine/Rendering/RenderPipeline/RenderPipeline.h"
#include "HuaEngine/Rendering/RenderPipeline/RenderResourceResolver.h"
#include "HuaEngine/Rendering/RHI/RenderTarget.h"
#include "Module/Rendering/SceneRenderExtractor.h"

namespace HE {
	class AssetResolver;

	class RenderSystem : public System {
	public:
		explicit RenderSystem(Ref<Scene> scene);
		SystemDescriptor Describe() const override;
		void Update(SystemContext& context) override;
		void Update() override;

		void RenderSingleCamera(World& world, Rendering::Camera& camera);
		[[nodiscard]] const Rendering::RenderResult& GetLastRenderResult() const { return m_LastRenderResult; }
		void SetCamera(Ref<Rendering::Camera>& camera) { m_Camera = camera; };
		void SetRenderTarget(const Ref<Rendering::RenderTarget>& renderTarget) { m_RenderTarget = renderTarget; }
		void SetAssetResolver(AssetResolver* resolver) { m_ResourceResolver.SetAssetResolver(resolver); }

	private:
		Ref<Scene> m_Scene;
		Ref<Rendering::Camera> m_Camera;
		Ref<Rendering::RenderTarget> m_RenderTarget;
		SceneRenderExtractor m_Extractor;
		Rendering::RenderResourceResolver m_ResourceResolver;
		Scope<Rendering::RenderPipeline> m_RenderPipeline;
		Rendering::RenderResult m_LastRenderResult;
	};
}
