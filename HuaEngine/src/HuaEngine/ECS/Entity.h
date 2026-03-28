#pragma once

#include <string>

#include "HuaEngine/Core/Core.h"
#include "entt.hpp"
#include "Components.h"
#include "EntityManager.h"

namespace HE {
	class Entity {
	public:
		Entity() = default;
		Entity(const Entity& other) = default;
		Entity(entt::entity handle, EntityManager* entityManager)
			:m_EntityHandle(handle), m_EntityManager(entityManager) {}

		template<typename T, typename... Args>
		T& AddComponent(Args&&... args) {
			T& component = m_EntityManager->m_Registry.template emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
			return component;
		}

		template<typename T>
		T& GetComponent() {
			T& component = m_EntityManager->m_Registry.template get<T>(m_EntityHandle);
			return component;
		}

		template<typename T>
		const T& GetComponent() const {
			const T& component = m_EntityManager->m_Registry.template get<T>(m_EntityHandle);
			return component;
		}

		template<typename T>
		bool HasComponent() const {
			return m_EntityManager->m_Registry.template all_of<T>(m_EntityHandle);
		}

		template<typename T>
		void RemoveComponent() {
			m_EntityManager->m_Registry.template remove<T>(m_EntityHandle);
		}

		std::string GetName() const {
			if (HasComponent<NameComponent>()) {
				return m_EntityManager->m_Registry.template get<NameComponent>(m_EntityHandle).Name;
			}

			return "Entity";
		}

		void SetName(const std::string& name) {
			if (HasComponent<NameComponent>()) {
				m_EntityManager->m_Registry.template get<NameComponent>(m_EntityHandle).Name = name;
				return;
			}

			m_EntityManager->m_Registry.template emplace<NameComponent>(m_EntityHandle, name);
		}

		operator entt::entity() const {
			return m_EntityHandle;
		}

		bool operator==(const Entity& other) const {
			return m_EntityHandle == other.m_EntityHandle && m_EntityManager == other.m_EntityManager;
		}

		bool operator!=(const Entity& other) const {
			return !(*this == other);
		}

		uint32_t GetUid() const { return (uint32_t)m_EntityHandle; }

		bool IsValid() const {
			return m_EntityManager != nullptr
				&& m_EntityHandle != entt::null
				&& m_EntityManager->m_Registry.valid(m_EntityHandle);
		}

	private:
		entt::entity m_EntityHandle;
		EntityManager* m_EntityManager;
		static uint32_t id;

		friend class EntityManager;
		friend class InspectorPanel;
	};
}
