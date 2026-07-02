#pragma once

#include "HuaEngine/ECS/System.h"
#include "HuaEngine/Scene/Scene.h"
#include "HuaEngine/Rendering/Renderer.h"
#include "HuaEngine/Rendering/RenderPipeline/RenderPipeline.h"
#include "HuaEngine/Rendering/RenderPipeline/RenderResourceResolver.h"
#include "HuaEngine/Rendering/RenderPipeline/SceneRenderExtractor.h"
#include "HuaEngine/Rendering/FrameBuffer.h"

namespace HE {
	class RenderSystem : public System {
	public:
		explicit RenderSystem(Ref<Scene> scene);
		SystemDescriptor Describe() const override;
		void Update(SystemContext& context) override;
		void Update() override;

		void RenderSingleCamera(World& world, Rendering::Camera& camera);
		[[nodiscard]] const Rendering::RenderResult& GetLastRenderResult() const { return m_LastRenderResult; }
		void SetCamera(Ref<Rendering::Camera>& camera) { m_Camera = camera; };
		void SetFrameBuffer(Ref<Rendering::FrameBuffer>& framebuffer) { m_Framebuffer = framebuffer; }

	private:
		Ref<Scene> m_Scene;
		Ref<Rendering::Camera> m_Camera;
		Ref<Rendering::FrameBuffer> m_Framebuffer;
		Rendering::SceneRenderExtractor m_Extractor;
		Rendering::RenderResourceResolver m_ResourceResolver;
		Scope<Rendering::RenderPipeline> m_RenderPipeline;
		Rendering::RenderResult m_LastRenderResult;
	};
}
