#include "enginepch.h"
#include "HuaEngine/ECS/Components.h"
#include "RenderingComponent.h"
#include "RenderSystem.h"
#include "HuaEngine/Rendering/RenderCommand.h"

namespace HE {
	SystemDescriptor RenderSystem::Describe() const {
		SystemDescriptor descriptor;
		descriptor.Name = "RenderSystem";
		descriptor.Stage = SystemStage::Render;
		descriptor.Reads = {
			ComponentTypeIdOf<TransformComponent>(),
			ComponentTypeIdOf<Rendering::CameraComponent>(),
			ComponentTypeIdOf<Rendering::MeshComponent>(),
			ComponentTypeIdOf<Rendering::MaterialComponent>()
		};
		return descriptor;
	}

	void RenderSystem::Update(SystemContext& context) {
		auto cameraQuery = context.WorldRef().Query<Rendering::CameraComponent>();
		cameraQuery.ForEach([&](Entity, Rendering::CameraComponent& camera) {
			if (camera.Camera) {
				RenderSingleCamera(context.WorldRef(), *camera.Camera);
			}
		});
	}

	void RenderSystem::Update() {
		if (!m_Scene) {
			return;
		}

		SystemContext context(m_Scene->GetWorld(), 0.0f);
		Update(context);
	}

	void RenderSystem::RenderSingleCamera(World& world, Rendering::Camera& camera) {
		if (!m_Framebuffer) {
			HE_CORE_WARN("RenderSystem::RenderSingleCamera skipped because no framebuffer is attached");
			return;
		}

		auto materialQuery = world.Query<TransformComponent, Rendering::MeshComponent, Rendering::MaterialComponent>();
		m_Framebuffer->Bind();
		Rendering::RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1.0f });
		Rendering::RenderCommand::Clear();

		Rendering::Renderer::Begin(std::make_shared<Rendering::Camera>(camera));

		materialQuery.ForEach([&](Entity, TransformComponent& transform, Rendering::MeshComponent& mesh, Rendering::MaterialComponent& materialComp) {
			auto vertexArray = mesh.GetVertexArray();
			if (materialComp.MaterialInstance && vertexArray) {
				Rendering::Renderer::Submit(materialComp.MaterialInstance, vertexArray, transform.GetTransformMat());
			}
		});

		Rendering::Renderer::End();
		m_Framebuffer->Unbind();
	}
}
