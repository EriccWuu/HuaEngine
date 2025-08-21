#pragma once

#include "HuaEngine/Core/Core.h"
#include "entt.hpp"
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
			T& component = m_EntityManager->m_Registry.emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
			return component;
		}

		template<typename T>
		T& GetComponent() {
			T& component = m_EntityManager->m_Registry.get<T>(m_EntityHandle);
			return component;
		}

		template<typename T>
		bool HasComponent() {
			return m_EntityManager->m_Registry.has<T>(m_EntityHandle);
		}

		template<typename T>
		void RemoveComponent() {
			m_EntityManager->m_Registry.remove<T>(m_EntityHandle);
		}

		std::string GetName() {
			return m_Name;
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

		uint32_t GetUid() { return (uint32_t)m_EntityHandle; }

		bool IsValid() { return m_EntityManager != nullptr; }

	private:
		entt::entity m_EntityHandle;
		EntityManager* m_EntityManager;
		std::string m_Name = "Entity";
		static uint32_t id;

		friend class EntityManager;
		friend class InspectorPanel;
	};
}