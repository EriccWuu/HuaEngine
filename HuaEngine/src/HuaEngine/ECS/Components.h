#pragma once

#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>

#include "glm/glm.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/quaternion.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include "HuaEngine/Reflection/Reflection.h"

namespace HE {
	struct Component {};

	struct NameComponent : Component {
		std::string Name = "Entity";

		NameComponent() = default;
		explicit NameComponent(std::string name)
			: Name(std::move(name)) {}
	};

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
		using InstantiateFunc = ScriptableEntity* (*)();
		using DestroyFunc = void (*)(NativeScriptComponent*);

		ScriptableEntity* Instance = nullptr;
		InstantiateFunc InstanceFunc = nullptr;
		DestroyFunc DestoryFunc = nullptr;
		std::string ScriptName;
		bool Enabled = true;
		bool HasCreated = false;

		template<typename T>
		void Bind(std::string_view scriptName = {}) {
			static_assert(std::is_base_of_v<ScriptableEntity, T>, "Native scripts must derive from ScriptableEntity");
			ReleaseInstance();
			InstanceFunc = []() -> ScriptableEntity* { return static_cast<T*>(new T()); };
			DestoryFunc = [](NativeScriptComponent* ncs) { delete ncs->Instance; ncs->Instance = nullptr; };
			ScriptName = scriptName.empty() ? typeid(T).name() : std::string(scriptName);
			Enabled = true;
			HasCreated = false;
		}

		[[nodiscard]] bool IsBound() const {
			return InstanceFunc != nullptr && DestoryFunc != nullptr;
		}

		void ReleaseInstance() {
			if (Instance && DestoryFunc) {
				DestoryFunc(this);
			}
			else {
				Instance = nullptr;
			}

			HasCreated = false;
		}

		void DestroyInstance() {
			ReleaseInstance();
		}
	};
}

srefl_class(NameComponent,
	fields(
		field(Name)
	)
)

srefl_class(TransformComponent,
	fields(
		field(Position),
		field(Rotation),
		field(Scale)
	)
)
