#include "enginepch.h"
#include "HuaEngine/ECS/Components.h"
#include "RenderingComponent.h"
#include "RenderSystem.h"
#include "HuaEngine/Rendering/RenderCommand.h"

namespace HE {
	void RenderSystem::Update() {
		auto cameraView = m_Scene->View<Rendering::CameraComponent>();
		for (auto cameraEntity : cameraView) {
			auto& camera = m_Scene->Get<Rendering::CameraComponent>(cameraEntity);
			RenderSingleCamera(*m_Scene, *camera.Camera);
		}
	}

	void RenderSystem::RenderSingleCamera(Scene& scene, Rendering::Camera& camera) {
		// Render objects using new material system
		auto materialEntityView = scene.View<TransformComponent, Rendering::MeshComponent, Rendering::MaterialComponent>();

		m_Framebuffer->Bind();
		Rendering::RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1.0f });
		Rendering::RenderCommand::Clear();

		Rendering::Renderer::Begin(std::make_shared<Rendering::Camera>(camera));

		// Render objects with new material system
		for (auto entity : materialEntityView) {
			auto [transform, mesh, materialComp] = materialEntityView.get<TransformComponent, Rendering::MeshComponent, Rendering::MaterialComponent>(entity);

			auto vertexArray = mesh.GetVertexArray();  // Uses lazy loading
			if (materialComp.MaterialInstance && vertexArray) {
				// Use new material rendering method
				Rendering::Renderer::Submit(materialComp.MaterialInstance, vertexArray, transform.GetTransformMat());
			}
		}

		Rendering::Renderer::End();
		m_Framebuffer->Unbind();
	}
}