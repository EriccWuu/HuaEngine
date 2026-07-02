#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "entt.hpp"
#include "HuaEngine/Core/Assert.h"
#include "HuaEngine/ECS/ComponentType.h"
#include "HuaEngine/ECS/Entity.h"
#include "HuaEngine/ECS/EntityId.h"

namespace HE {
	template<typename... Terms>
	class Query;

	class World {
	public:
		World() = default;

		Entity CreateEntity(const std::string& name = "Entity");
		Entity CreateEntityWithUuid(EntityUuid uuid, std::string_view name);
		void DestroyEntity(EntityId id);
		void Clear();

		[[nodiscard]] bool IsAlive(EntityId id) const;
		[[nodiscard]] size_t GetEntityCount() const { return m_EntityCount; }
		[[nodiscard]] EntityId FindEntity(EntityUuid uuid) const;
		[[nodiscard]] Entity GetEntity(EntityId id);
		[[nodiscard]] Entity GetEntity(EntityUuid uuid);
		[[nodiscard]] Entity GetEntityByIndex(uint32_t index);
		[[nodiscard]] EntityUuid GetUuid(EntityId id) const;
		[[nodiscard]] EntityUuid GetEntityUuid(EntityId id) const { return GetUuid(id); }
		[[nodiscard]] std::string GetEntityName(EntityId id) const;
		void SetEntityName(EntityId id, const std::string& name);
		[[nodiscard]] const void* TryGetComponentByType(EntityId id, ComponentTypeId typeId) const;
		[[nodiscard]] void* TryGetComponentByType(EntityId id, ComponentTypeId typeId);
		[[nodiscard]] std::vector<ComponentTypeId> ListComponentTypes(EntityId id) const;

		template<typename Callback>
		void ForEachEntity(Callback&& callback) {
			for (uint32_t index = 0; index < m_Records.size(); ++index) {
				const auto& record = m_Records[index];
				const EntityId id{index, record.Generation};
				if (!IsAlive(id)) {
					continue;
				}

				callback(Entity(id, this));
			}
		}

		template<typename T, typename... Args>
		T& AddComponent(EntityId id, Args&&... args) {
			using ComponentType = typename Detail::CleanComponentType<T>;
			EnsureComponentAccessors<ComponentType>();

			auto* record = TryGetRecord(id);
			HE_CORE_ASSERT(record != nullptr, "Cannot add a component to an invalid entity");
			TrackComponentType(id, ComponentTypeIdOf<ComponentType>());
			return m_Registry.emplace_or_replace<ComponentType>(record->RawEntity, std::forward<Args>(args)...);
		}

		template<typename T>
		T* TryGetComponent(EntityId id) {
			using ComponentType = typename Detail::CleanComponentType<T>;
			auto* record = TryGetRecord(id);
			if (record == nullptr || !m_Registry.all_of<ComponentType>(record->RawEntity)) {
				return nullptr;
			}

			return &m_Registry.get<ComponentType>(record->RawEntity);
		}

		template<typename T>
		const T* TryGetComponent(EntityId id) const {
			using ComponentType = typename Detail::CleanComponentType<T>;
			const auto* record = TryGetRecord(id);
			if (record == nullptr || !m_Registry.all_of<ComponentType>(record->RawEntity)) {
				return nullptr;
			}

			return &m_Registry.get<ComponentType>(record->RawEntity);
		}

		template<typename T>
		bool HasComponent(EntityId id) const {
			return TryGetComponent<T>(id) != nullptr;
		}

		template<typename T>
		void RemoveComponent(EntityId id) {
			using ComponentType = typename Detail::CleanComponentType<T>;
			auto* record = TryGetRecord(id);
			if (record == nullptr || !m_Registry.all_of<ComponentType>(record->RawEntity)) {
				return;
			}

			m_Registry.remove<ComponentType>(record->RawEntity);
			UntrackComponentType(id, ComponentTypeIdOf<ComponentType>());
		}

		template<typename... Terms>
		HE::Query<Terms...> Query();

	private:
		struct EntityRecord {
			uint32_t Generation = 0;
			bool Alive = false;
			EntityUuid Uuid;
			std::string Name = "Entity";
			entt::entity RawEntity = entt::null;
		};

		struct ComponentAccessors {
			void* (*TryGetRaw)(entt::registry&, entt::entity) = nullptr;
			const void* (*TryGetRawConst)(const entt::registry&, entt::entity) = nullptr;
		};

		template<typename T>
		void EnsureComponentAccessors() {
			using ComponentType = typename Detail::CleanComponentType<T>;
			const ComponentTypeId typeId = ComponentTypeIdOf<ComponentType>();
			if (m_ComponentAccessors.find(typeId) != m_ComponentAccessors.end()) {
				return;
			}

			m_ComponentAccessors.emplace(typeId, ComponentAccessors{
				.TryGetRaw = [](entt::registry& registry, entt::entity entity) -> void* {
					return registry.all_of<ComponentType>(entity) ? &registry.get<ComponentType>(entity) : nullptr;
				},
				.TryGetRawConst = [](const entt::registry& registry, entt::entity entity) -> const void* {
					return registry.all_of<ComponentType>(entity) ? &registry.get<ComponentType>(entity) : nullptr;
				}
			});
		}

		[[nodiscard]] EntityRecord* TryGetRecord(EntityId id);
		[[nodiscard]] const EntityRecord* TryGetRecord(EntityId id) const;
		void TrackComponentType(EntityId id, ComponentTypeId typeId);
		void UntrackComponentType(EntityId id, ComponentTypeId typeId);

		[[nodiscard]] EntityUuid GenerateUuid();

	private:
		std::vector<EntityRecord> m_Records;
		std::vector<uint32_t> m_FreeIndices;
		std::unordered_map<EntityUuid, EntityId> m_UuidToEntity;
		std::unordered_map<ComponentTypeId, ComponentAccessors> m_ComponentAccessors;
		std::unordered_map<EntityId, std::vector<ComponentTypeId>> m_ComponentTypesByEntity;
		entt::registry m_Registry;
		size_t m_EntityCount = 0;
		uint64_t m_NextUuid = 1;
	};
}

#include "HuaEngine/ECS/Query.h"

namespace HE {
	template<typename... Terms>
	HE::Query<Terms...> World::Query() {
		return HE::Query<Terms...>(*this);
	}

	template<typename T, typename... Args>
	T& Entity::AddComponent(Args&&... args) {
		return m_World->AddComponent<T>(m_Id, std::forward<Args>(args)...);
	}

	template<typename T>
	T& Entity::GetComponent() {
		return *m_World->TryGetComponent<T>(m_Id);
	}

	template<typename T>
	const T& Entity::GetComponent() const {
		return *m_World->TryGetComponent<T>(m_Id);
	}

	template<typename T>
	bool Entity::HasComponent() const {
		return m_World != nullptr && m_World->HasComponent<T>(m_Id);
	}

	template<typename T>
	void Entity::RemoveComponent() {
		if (m_World) {
			m_World->RemoveComponent<T>(m_Id);
		}
	}

	template<typename T>
	T* Entity::TryGetComponent() {
		return m_World != nullptr ? m_World->TryGetComponent<T>(m_Id) : nullptr;
	}

	template<typename T>
	const T* Entity::TryGetComponent() const {
		return m_World != nullptr ? m_World->TryGetComponent<T>(m_Id) : nullptr;
	}

	inline void* Entity::TryGetComponentByType(ComponentTypeId typeId) {
		return m_World != nullptr ? m_World->TryGetComponentByType(m_Id, typeId) : nullptr;
	}

	inline const void* Entity::TryGetComponentByType(ComponentTypeId typeId) const {
		return m_World != nullptr ? m_World->TryGetComponentByType(m_Id, typeId) : nullptr;
	}

	inline std::vector<ComponentTypeId> Entity::ListComponentTypes() const {
		return m_World != nullptr ? m_World->ListComponentTypes(m_Id) : std::vector<ComponentTypeId>{};
	}
}
