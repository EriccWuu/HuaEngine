#pragma once

#include "HuaEngine/ECS/System.h"
#include "HuaEngine/Rendering/RenderCamera.h"
#include "HuaEngine/Scene/Scene.h"

#include <cstdint>

namespace HE {
	class CameraSystem final : public System {
	public:
		explicit CameraSystem(Ref<Scene> scene);

		SystemDescriptor Describe() const override;
		void Update(SystemContext& context) override;
		void Update() override;

		void SetRenderViewportSize(uint32_t width, uint32_t height) {
			m_ViewportWidth = width;
			m_ViewportHeight = height;
		}
		[[nodiscard]] const Ref<Rendering::RenderCamera>& GetActiveCamera() const { return m_ActiveCamera; }

	private:
		Ref<Scene> m_Scene;
		uint32_t m_ViewportWidth = 0;
		uint32_t m_ViewportHeight = 0;
		Ref<Rendering::RenderCamera> m_ActiveCamera;
	};
}
