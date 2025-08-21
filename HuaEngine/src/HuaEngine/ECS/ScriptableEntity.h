#pragma once

#include "Entity.h"

namespace HE {
	class ScriptableEntity {
	public:
		virtual ~ScriptableEntity() {}

		template<typename T, typename... Args>
		T& AddComponent(Args&&... args) {
			T& component = m_Entity.AddComponent<T>(std::forward<Args>(args)...);
			return component;
		}

		template<typename T>
		T& GetComponent() {
			T& component = m_Entity.GetComponent<T>(m_EntityHandle);
			return component;
		}

		template<typename T>
		bool HasComponent() {
			return m_Entity.HasComponent<T>(m_EntityHandle);
		}

		template<typename T>
		void RemoveComponent() {
			m_Entity.RemoveComponent<T>();
		}

	protected:
		virtual void OnUpdate() {}
		virtual void OnCreate() {}
		virtual void OnDestory() {}

	private:
		Entity* m_Entity;
	};
}