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
		// 渲染使用新材质系统的对象
		auto& materialEntityView = scene.View<TransformComponent, MeshComponent, MaterialComponent>();
		
		m_Framebuffer->Bind();
		RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1.0f });
		RenderCommand::Clear();
		
		Renderer::Begin(std::make_shared<Camera>(camera));

		// 渲染新材质系统的对象
		for (auto entity : materialEntityView) {
			auto [transform, mesh, materialComp] = materialEntityView.get<TransformComponent, MeshComponent, MaterialComponent>(entity);

			auto vertexArray = mesh.GetVertexArray();  // 使用延迟加载
			if (materialComp.MaterialInstance && vertexArray) {
				// 使用新的材质渲染方法
				Renderer::Submit(materialComp.MaterialInstance, vertexArray, transform.GetTransformMat());
			}
		}

		Renderer::End();
		m_Framebuffer->Unbind();
	}
}