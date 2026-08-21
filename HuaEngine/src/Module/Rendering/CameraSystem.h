#pragma once

#include "HuaEngine/ECS/System.h"
#include "HuaEngine/Rendering/RHI/RenderTarget.h"
#include "HuaEngine/Rendering/RenderCamera.h"
#include "HuaEngine/Scene/Scene.h"

namespace HE {
	class CameraSystem final : public System {
	public:
		explicit CameraSystem(Ref<Scene> scene);

		SystemDescriptor Describe() const override;
		void Update(SystemContext& context) override;
		void Update() override;

		void SetRenderTarget(const Ref<Rendering::RenderTarget>& renderTarget) { m_RenderTarget = renderTarget; }
		[[nodiscard]] const Ref<Rendering::RenderCamera>& GetActiveCamera() const { return m_ActiveCamera; }

	private:
		Ref<Scene> m_Scene;
		Ref<Rendering::RenderTarget> m_RenderTarget;
		Ref<Rendering::RenderCamera> m_ActiveCamera;
	};
}
