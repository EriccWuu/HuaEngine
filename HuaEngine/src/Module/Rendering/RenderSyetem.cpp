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
		
		// 渲染使用旧组件系统的对象（向后兼容）
		auto& rendererEntityView = scene.View<TransformComponent, MeshComponent, RendererComponent>();
		
		m_Framebuffer->Bind();
		RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1.0f });
		RenderCommand::Clear();
		
		Renderer::Begin(std::make_shared<Camera>(camera));

		// 渲染新材质系统的对象
		for (auto entity : materialEntityView) {
			auto [transform, mesh, materialComp] = materialEntityView.get<TransformComponent, MeshComponent, MaterialComponent>(entity);

			if (materialComp.MaterialInstance && mesh.VertexArray) {
				// 使用新的材质渲染方法
				Renderer::Submit(materialComp.MaterialInstance, mesh.VertexArray, transform.GetTransformMat());
			}
		}

		// 渲染旧组件系统的对象（向后兼容）
		for (auto entity : rendererEntityView) {
			auto [transform, mesh, renderer] = rendererEntityView.get<TransformComponent, MeshComponent, RendererComponent>(entity);

			if (renderer.Shader && mesh.VertexArray) {
				renderer.Shader->Bind();
				if (renderer.Texture) {
					renderer.Texture->Bind(0);
					renderer.Shader->SetInt("u_Texture", 0);
				}
				Renderer::Submit(renderer.Shader, mesh.VertexArray, transform.GetTransformMat());
				renderer.Shader->Unbind();
			}
		}

		Renderer::End();
		m_Framebuffer->Unbind();
	}
}