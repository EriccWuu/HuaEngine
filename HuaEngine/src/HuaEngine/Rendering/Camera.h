#pragma once

#include "HuaEngine/Rendering/RenderCamera.h"

namespace HE::Rendering {
	class Camera : public RenderCamera {
	public:
		Camera() = default;
		Camera(const Camera& other) = default;
		Camera(const glm::mat4& projection): RenderCamera(projection, glm::mat4(1.0f)) {}
		virtual ~Camera() = default;

		virtual void OnUpdate() {};

	};
}
