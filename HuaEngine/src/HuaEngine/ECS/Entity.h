#pragma once

#include <string>
#include <utility>

#include "HuaEngine/Core/Core.h"
#include "entt.hpp"
#include "Components.h"
#include "EntityId.h"
#include "EntityManager.h"

namespace HE {
	class World;

	namespace Detail {
		EntityUuid GetWorldEntityUuid(const World* world, EntityId id);
		std::string GetWorldEntityName(const World* world, EntityId id);
		void SetWorldEntityName(World* world, EntityId id, const std::string& name);
		bool IsWorldEntityAlive(const World* world, EntityId id);
	}

	class Entity {
	public:
		Entity() = default;
		Entity(const Entity& other) = default;
		Entity(entt::entity handle, EntityManager* entityManager)
			:m_EntityHandle(handle), m_EntityManager(entityManager) {}
		Entity(EntityId id, World* world)
			: m_Id(id), m_World(world) {}

		template<typename T, typename... Args>
		T& AddComponent(Args&&... args);

		template<typename T>
		T& GetComponent();

		template<typename T>
		const T& GetComponent() const;

		template<typename T>
		bool HasComponent() const;

		template<typename T>
		void RemoveComponent();

		template<typename T>
		T* TryGetComponent();

		template<typename T>
		const T* TryGetComponent() const;

		EntityId GetId() const { return m_Id; }
		EntityUuid GetUuid() const {
			return m_World != nullptr ? Detail::GetWorldEntityUuid(m_World, m_Id) : EntityUuid{};
		}

		std::string GetName() const {
			if (m_World != nullptr) {
				return Detail::GetWorldEntityName(m_World, m_Id);
			}

			if (HasComponent<NameComponent>()) {
				return m_EntityManager->m_Registry.template get<NameComponent>(m_EntityHandle).Name;
			}

			return "Entity";
		}

		void SetName(const std::string& name) {
			if (m_World != nullptr) {
				Detail::SetWorldEntityName(m_World, m_Id, name);
				return;
			}

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
			return m_EntityHandle == other.m_EntityHandle
				&& m_EntityManager == other.m_EntityManager
				&& m_Id == other.m_Id
				&& m_World == other.m_World;
		}

		bool operator!=(const Entity& other) const {
			return !(*this == other);
		}

		uint32_t GetUid() const { return m_World != nullptr ? m_Id.Index : (uint32_t)m_EntityHandle; }

		bool IsValid() const {
			if (m_World != nullptr) {
				return Detail::IsWorldEntityAlive(m_World, m_Id);
			}

			return m_EntityManager != nullptr
				&& m_EntityHandle != entt::null
				&& m_EntityManager->m_Registry.valid(m_EntityHandle);
		}

	private:
		entt::entity m_EntityHandle = entt::null;
		EntityManager* m_EntityManager = nullptr;
		EntityId m_Id;
		World* m_World = nullptr;
		static uint32_t id;

		friend class EntityManager;
		friend class InspectorPanel;
		friend class World;
	};
}

#include "HuaEngine/ECS/World.h"
