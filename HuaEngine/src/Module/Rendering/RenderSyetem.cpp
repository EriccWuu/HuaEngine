#include "enginepch.h"
#include "HuaEngine/ECS/Components.h"
#include "RenderingComponent.h"
#include "RenderSystem.h"
#include "HuaEngine/Rendering/RenderCommand.h"

namespace HE {
	void RenderSystem::Update() {
		auto& cameraView = m_Scene->View<CameraComponent>();
		for (auto cameraEntity : cameraView) {
			auto& camera = m_Scene->Get<CameraComponent>(cameraEntity);
			RenderSingleCamera(*m_Scene, *camera.Camera);
		}
	}

	void RenderSystem::RenderSingleCamera(Scene& scene, Camera& camera) {
		auto& entityView = scene.View<TransformComponent, MeshComponent, RendererComponent>();
		m_Framebuffer->Bind();
		RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1.0f });
		RenderCommand::Clear();
		for (auto entity : entityView) {
			auto [transform, mesh, renderer] = entityView.get<TransformComponent, MeshComponent, RendererComponent>(entity);

			Renderer::Begin(std::make_shared<Camera>(camera));

			renderer.Texture->Bind(0);

			renderer.Shader->Bind();
			renderer.Shader->SetInt("u_Texture", 0);
			Renderer::Submit(renderer.Shader, mesh.VertexArray, transform.GetTransformMat());

			Renderer::End();
		}
		m_Framebuffer->Unbind();
	}
}