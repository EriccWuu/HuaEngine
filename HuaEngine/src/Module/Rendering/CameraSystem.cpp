#include "enginepch.h"
#include "CameraSystem.h"

#include "HuaEngine/ECS/Components.h"
#include "RenderingComponent.h"

namespace HE {
	CameraSystem::CameraSystem(Ref<Scene> scene)
		: m_Scene(std::move(scene)) {}

	SystemDescriptor CameraSystem::Describe() const {
		SystemDescriptor descriptor;
		descriptor.Name = "CameraSystem";
		descriptor.Stage = SystemStage::Render;
		descriptor.Before = { "RenderSystem" };
		descriptor.Reads = {
			ComponentTypeIdOf<TransformComponent>(),
			ComponentTypeIdOf<Rendering::CameraComponent>()
		};
		return descriptor;
	}

	void CameraSystem::Update(SystemContext& context) {
		m_ActiveCamera.reset();

		auto cameraQuery = context.WorldRef().Query<TransformComponent, Rendering::CameraComponent>();
		cameraQuery.ForEach([this](Entity, TransformComponent& transform, Rendering::CameraComponent& camera) {
			if (m_ActiveCamera || !camera.Primary) {
				return;
			}

			const float aspectRatio = camera.FixedAspectRatio || !m_RenderTarget
				? camera.AspectRatio
				: static_cast<float>(m_RenderTarget->GetSpecification().Width) / static_cast<float>(m_RenderTarget->GetSpecification().Height);
			const auto projection = glm::perspective(
				glm::radians(camera.VerticalFovDegrees),
				aspectRatio,
				camera.NearClip,
				camera.FarClip);
			m_ActiveCamera = CreateRef<Rendering::RenderCamera>(projection, glm::inverse(transform.GetTransformMat()));
		});
	}

	void CameraSystem::Update() {
		if (!m_Scene) {
			return;
		}

		SystemContext context(m_Scene->GetWorld(), 0.0f);
		Update(context);
	}
}
