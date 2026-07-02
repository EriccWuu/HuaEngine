#include "enginepch.h"
#include "HuaEngine/ECS/Components.h"
#include "RenderingComponent.h"
#include "RenderSystem.h"

namespace HE {
	RenderSystem::RenderSystem(Ref<Scene> scene)
		: m_Scene(std::move(scene)), m_RenderPipeline(CreateScope<Rendering::RenderPipeline>()) {}

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
		m_LastRenderResult = {};
		if (!m_Framebuffer) {
			HE_CORE_WARN("RenderSystem::RenderSingleCamera skipped because no framebuffer is attached");
			return;
		}

		Rendering::RenderView view;
		view.CameraRef = CreateRef<Rendering::Camera>(camera);
		view.Target = m_Framebuffer;

		auto renderItems = m_Extractor.Extract(world);
		m_LastRenderResult = m_RenderPipeline->Render(view, renderItems, m_ResourceResolver);
	}
}
