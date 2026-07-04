#pragma once

#include "glm/glm.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/quaternion.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include "HuaEngine/Reflection/Reflection.h"
#include "HuaEngine/Reflection/ReflectionMarkers.h"

namespace HE {
	HE_REFLECT_COMPONENT(DisplayName="Transform", Category="Core")
	struct TransformComponent : Component {
		HE_REFLECT_FIELD()
		glm::vec3 Position = {0.0f, 0.0f, 0.0f};
		HE_REFLECT_FIELD()
		glm::vec3 Rotation = {0.0f, 0.0f, 0.0f};
		HE_REFLECT_FIELD()
		glm::vec3 Scale = {1.0f, 1.0f, 1.0f};

		TransformComponent() = default;
		TransformComponent(const glm::vec3& position)
			: Position(position) {}

		glm::mat4 GetTransformMat() const {
			glm::mat4 rotation = glm::toMat4(glm::quat(Rotation));
			return glm::translate(glm::mat4(1.0f), Position) *
					rotation *
					glm::scale(glm::mat4(1.0f), Scale);
		}
	};
}
