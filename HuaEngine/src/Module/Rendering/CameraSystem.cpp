#include "enginepch.h"
#include "CameraSystem.h"

#include "HuaEngine/ECS/Components.h"
#include "RenderFrameData.h"
#include "RenderingComponent.h"

namespace HE {
	SystemDescriptor CameraSystem::Describe() const {
		SystemDescriptor descriptor;
		descriptor.Name = "CameraSystem";
		descriptor.Stage = SystemStage::Render;
		descriptor.Before = { "RenderSystem" };
		descriptor.Reads = {
			ComponentTypeIdOf<TransformComponent>(),
			ComponentTypeIdOf<Rendering::CameraComponent>()
		};
		descriptor.ResourceWrites = { std::string(Rendering::RenderFrameData::ResourceName) };
		return descriptor;
	}

	void CameraSystem::Update(SystemContext& context) {
		auto& renderFrame = context.Frame().GetOrCreate<Rendering::RenderFrameData>();
		renderFrame.ActiveCamera.reset();

		auto cameraQuery = context.WorldRef().Query<TransformComponent, Rendering::CameraComponent>();
		cameraQuery.ForEach([this, &renderFrame](Entity, TransformComponent& transform, Rendering::CameraComponent& camera) {
			if (renderFrame.ActiveCamera || !camera.Primary) {
				return;
			}

			const float aspectRatio = camera.FixedAspectRatio || m_ViewportWidth == 0 || m_ViewportHeight == 0
				? camera.AspectRatio
				: static_cast<float>(m_ViewportWidth) / static_cast<float>(m_ViewportHeight);
			const auto projection = glm::perspective(
				glm::radians(camera.VerticalFovDegrees),
				aspectRatio,
				camera.NearClip,
				camera.FarClip);
			renderFrame.ActiveCamera.emplace(projection, glm::inverse(transform.GetTransformMat()));
		});
	}
}
