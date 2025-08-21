#pragma once

#include "HuaEngine/ECS/Syetem.h"
#include "HuaEngine/Scene/Scene.h"
#include "HuaEngine/Rendering/Renderer.h"
#include "HuaEngine/Rendering/RenderPipeline/RenderPipeline.h"
#include "HuaEngine/Rendering/FrameBuffer.h"

namespace HE {
	class RenderSystem : public System {
	public:
		RenderSystem(Ref<Scene> scene): m_Scene(scene) {}
		virtual void Update() override;

		void RenderSingleCamera(Scene& scene, Camera& camera);
		void SetCamera(Ref<Camera>& camera) { m_Camera = camera; };
		void SetFrameBuffer(Ref<FrameBuffer>& framebuffer) { m_Framebuffer = framebuffer; }

	private:
		Ref<Scene> m_Scene;
		Ref<Camera> m_Camera;
		Ref<FrameBuffer> m_Framebuffer;
	};
}