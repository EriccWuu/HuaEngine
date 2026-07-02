#pragma once

#include <string>
#include <utility>
#include <vector>

#include "HuaEngine/Core/Core.h"
#include "Components.h"
#include "ComponentType.h"
#include "EntityId.h"

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

		[[nodiscard]] void* TryGetComponentByType(ComponentTypeId typeId);
		[[nodiscard]] const void* TryGetComponentByType(ComponentTypeId typeId) const;
		[[nodiscard]] std::vector<ComponentTypeId> ListComponentTypes() const;

		EntityId GetId() const { return m_Id; }
		EntityUuid GetUuid() const {
			return Detail::GetWorldEntityUuid(m_World, m_Id);
		}

		std::string GetName() const {
			return Detail::GetWorldEntityName(m_World, m_Id);
		}

		void SetName(const std::string& name) {
			Detail::SetWorldEntityName(m_World, m_Id, name);
		}

		bool operator==(const Entity& other) const {
			return m_Id == other.m_Id
				&& m_World == other.m_World;
		}

		bool operator!=(const Entity& other) const {
			return !(*this == other);
		}

		uint32_t GetUid() const { return m_Id.Index; }

		bool IsValid() const {
			return Detail::IsWorldEntityAlive(m_World, m_Id);
		}

	private:
		EntityId m_Id;
		World* m_World = nullptr;
		static uint32_t id;

		friend class InspectorPanel;
		friend class World;
	};
}

#include "HuaEngine/ECS/World.h"
