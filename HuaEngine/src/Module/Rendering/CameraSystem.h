#pragma once

#include "HuaEngine/ECS/System.h"
#include <cstdint>

namespace HE {
	class CameraSystem final : public System {
	public:
		SystemDescriptor Describe() const override;
		void Update(SystemContext& context) override;

		void SetRenderViewportSize(uint32_t width, uint32_t height) {
			m_ViewportWidth = width;
			m_ViewportHeight = height;
		}
	private:
		uint32_t m_ViewportWidth = 0;
		uint32_t m_ViewportHeight = 0;
	};
}
