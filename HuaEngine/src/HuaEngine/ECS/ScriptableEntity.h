#pragma once

#include "Entity.h"
#include "HuaEngine/Core/Assert.h"

namespace HE {
	class ScriptService;
	class ScriptRuntimeSystem;

	class ScriptableEntity {
	public:
		virtual ~ScriptableEntity() {}

		template<typename T, typename... Args>
		T& AddComponent(Args&&... args) {
			HE_CORE_ASSERT(m_Entity.IsValid(), "ScriptableEntity is not bound to an entity");
			return m_Entity.AddComponent<T>(std::forward<Args>(args)...);
		}

		template<typename T>
		T& GetComponent() {
			HE_CORE_ASSERT(m_Entity.IsValid(), "ScriptableEntity is not bound to an entity");
			return m_Entity.GetComponent<T>();
		}

		template<typename T>
		bool HasComponent() {
			HE_CORE_ASSERT(m_Entity.IsValid(), "ScriptableEntity is not bound to an entity");
			return m_Entity.HasComponent<T>();
		}

		template<typename T>
		void RemoveComponent() {
			HE_CORE_ASSERT(m_Entity.IsValid(), "ScriptableEntity is not bound to an entity");
			m_Entity.RemoveComponent<T>();
		}

	protected:
		virtual void OnUpdate() {}
		virtual void OnCreate() {}
		virtual void OnDestroy() { OnDestory(); }
		virtual void OnDestory() {}

	private:
		void __BindEntity(Entity entity) {
			m_Entity = entity;
		}

		void __OnCreate() {
			OnCreate();
		}

		void __OnUpdate() {
			OnUpdate();
		}

		void __OnDestroy() {
			OnDestroy();
			m_Entity = {};
		}

		Entity m_Entity;

		friend class ScriptService;
		friend class ScriptRuntimeSystem;
	};
}
