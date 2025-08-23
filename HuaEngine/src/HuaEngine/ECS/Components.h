#pragma once

#include "glm/glm.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/quaternion.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include "HuaEngine/Reflection/Reflection.h"

namespace HE {
	struct Component {};

	struct TransformComponent : Component {
		glm::vec3 Position = {0.0f, 0.0f, 0.0f};
		glm::vec3 Rotation = {0.0f, 0.0f, 0.0f};
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



	class ScriptableEntity;

	struct NativeScriptComponent : Component {
		ScriptableEntity* Instance = nullptr;
		ScriptableEntity* (*InstanceFunc)();
		void (*DestoryFunc)(NativeScriptComponent*);

		template<typename T>
		void Bind() {
			InstanceFunc = []() { return static_cast<T*>(new T());  };
			DestoryFunc = [](NativeScriptComponent* ncs) { delete ncs->Instance; ncs->Instance = nullptr; }
		}
	};
}

srefl_class(TransformComponent,
	fields(
		field(Position),
		field(Rotation),
		field(Scale)
	)
)