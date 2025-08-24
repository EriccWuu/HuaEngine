#include "enginepch.h"
#include "HuaEngine/ECS/Components.h"
#include "RenderingComponent.h"
#include "RenderSystem.h"
#include "HuaEngine/Rendering/RenderCommand.h"

namespace HE {
	void RenderSystem::Update() {
		auto& cameraView = m_Scene->View<Rendering::CameraComponent>();
		for (auto cameraEntity : cameraView) {
			auto& camera = m_Scene->Get<Rendering::CameraComponent>(cameraEntity);
			RenderSingleCamera(*m_Scene, *camera.Camera);
		}
	}

	void RenderSystem::RenderSingleCamera(Scene& scene, Rendering::Camera& camera) {
		// 渲染使用新材质系统的对象
		auto& materialEntityView = scene.View<TransformComponent, Rendering::MeshComponent, Rendering::MaterialComponent>();
		
		m_Framebuffer->Bind();
		Rendering::RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1.0f });
		Rendering::RenderCommand::Clear();
		
		Rendering::Renderer::Begin(std::make_shared<Rendering::Camera>(camera));

		// 渲染新材质系统的对象
		for (auto entity : materialEntityView) {
			auto [transform, mesh, materialComp] = materialEntityView.get<TransformComponent, Rendering::MeshComponent, Rendering::MaterialComponent>(entity);

			auto vertexArray = mesh.GetVertexArray();  // 使用延迟加载
			if (materialComp.MaterialInstance && vertexArray) {
				// 使用新的材质渲染方法
				Rendering::Renderer::Submit(materialComp.MaterialInstance, vertexArray, transform.GetTransformMat());
			}
		}

		Rendering::Renderer::End();
		m_Framebuffer->Unbind();
	}
}