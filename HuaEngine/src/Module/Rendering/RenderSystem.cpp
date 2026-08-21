#include "enginepch.h"
#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/Rendering/RenderPipeline/ForwardRenderPipeline.h"
#include "RenderingComponent.h"
#include "RenderSystem.h"

namespace HE {
	RenderSystem::RenderSystem(Ref<Scene> scene)
		: m_Scene(std::move(scene)), m_RenderPipeline(CreateScope<Rendering::ForwardRenderPipeline>()) {}

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
		auto cameraQuery = context.WorldRef().Query<TransformComponent, Rendering::CameraComponent>();
		cameraQuery.ForEach([&](Entity, TransformComponent& transform, Rendering::CameraComponent& camera) {
			if (camera.Primary) {
				const float aspectRatio = camera.FixedAspectRatio || !m_RenderTarget
					? camera.AspectRatio
					: static_cast<float>(m_RenderTarget->GetSpecification().Width) / static_cast<float>(m_RenderTarget->GetSpecification().Height);
				const auto projection = glm::perspective(glm::radians(camera.VerticalFovDegrees), aspectRatio, camera.NearClip, camera.FarClip);
				RenderSingleCamera(context.WorldRef(), Rendering::RenderCamera(projection, glm::inverse(transform.GetTransformMat())));
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

	void RenderSystem::RenderSingleCamera(World& world, const Rendering::RenderCamera& camera) {
		m_LastRenderResult = {};
		if (!m_RenderTarget) {
			HE_CORE_WARN("RenderSystem::RenderSingleCamera skipped because no render target is attached");
			return;
		}

		Rendering::RenderView view;
		view.CameraRef = CreateRef<Rendering::RenderCamera>(camera);
		view.Target = m_RenderTarget;

		auto renderItems = m_Extractor.Extract(world);
		m_LastRenderResult = m_RenderPipeline->Render(view, renderItems, m_ResourceResolver);
	}
}
