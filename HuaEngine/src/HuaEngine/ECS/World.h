#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

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
		void DestroyEntity(EntityId id);

		[[nodiscard]] bool IsAlive(EntityId id) const;
		[[nodiscard]] size_t GetEntityCount() const { return m_EntityCount; }
		[[nodiscard]] EntityId FindEntity(EntityUuid uuid) const;
		[[nodiscard]] EntityUuid GetUuid(EntityId id) const;
		[[nodiscard]] EntityUuid GetEntityUuid(EntityId id) const { return GetUuid(id); }
		[[nodiscard]] std::string GetEntityName(EntityId id) const;
		void SetEntityName(EntityId id, const std::string& name);

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
			auto& storage = GetOrCreateStorage<T>();
			T component{std::forward<Args>(args)...};
			auto [iterator, inserted] = storage.Components.insert_or_assign(id, std::move(component));
			(void)inserted;
			return iterator->second;
		}

		template<typename T>
		T* TryGetComponent(EntityId id) {
			if (!IsAlive(id)) {
				return nullptr;
			}

			auto* storage = GetStorage<T>();
			if (storage == nullptr) {
				return nullptr;
			}

			auto iterator = storage->Components.find(id);
			if (iterator == storage->Components.end()) {
				return nullptr;
			}

			return &iterator->second;
		}

		template<typename T>
		const T* TryGetComponent(EntityId id) const {
			if (!IsAlive(id)) {
				return nullptr;
			}

			const auto* storage = GetStorage<T>();
			if (storage == nullptr) {
				return nullptr;
			}

			auto iterator = storage->Components.find(id);
			if (iterator == storage->Components.end()) {
				return nullptr;
			}

			return &iterator->second;
		}

		template<typename T>
		bool HasComponent(EntityId id) const {
			return TryGetComponent<T>(id) != nullptr;
		}

		template<typename T>
		void RemoveComponent(EntityId id) {
			auto* storage = GetStorage<T>();
			if (storage == nullptr) {
				return;
			}

			storage->Components.erase(id);
		}

		template<typename... Terms>
		HE::Query<Terms...> Query();

	private:
		struct EntityRecord {
			uint32_t Generation = 0;
			bool Alive = false;
			EntityUuid Uuid;
			std::string Name = "Entity";
		};

		struct IComponentStorage {
			virtual ~IComponentStorage() = default;
		};

		template<typename T>
		struct ComponentStorage final : IComponentStorage {
			std::unordered_map<EntityId, T> Components;
		};

		template<typename T>
		ComponentStorage<typename Detail::CleanComponentType<T>>& GetOrCreateStorage() {
			using ComponentType = typename Detail::CleanComponentType<T>;
			const ComponentTypeId typeId = ComponentTypeIdOf<ComponentType>();

			auto iterator = m_ComponentStorages.find(typeId);
			if (iterator == m_ComponentStorages.end()) {
				auto storage = std::make_unique<ComponentStorage<ComponentType>>();
				auto* storagePointer = storage.get();
				m_ComponentStorages.emplace(typeId, std::move(storage));
				return *storagePointer;
			}

			return *static_cast<ComponentStorage<ComponentType>*>(iterator->second.get());
		}

		template<typename T>
		ComponentStorage<typename Detail::CleanComponentType<T>>* GetStorage() {
			using ComponentType = typename Detail::CleanComponentType<T>;
			const ComponentTypeId typeId = ComponentTypeIdOf<ComponentType>();

			auto iterator = m_ComponentStorages.find(typeId);
			if (iterator == m_ComponentStorages.end()) {
				return nullptr;
			}

			return static_cast<ComponentStorage<ComponentType>*>(iterator->second.get());
		}

		template<typename T>
		const ComponentStorage<typename Detail::CleanComponentType<T>>* GetStorage() const {
			using ComponentType = typename Detail::CleanComponentType<T>;
			const ComponentTypeId typeId = ComponentTypeIdOf<ComponentType>();

			auto iterator = m_ComponentStorages.find(typeId);
			if (iterator == m_ComponentStorages.end()) {
				return nullptr;
			}

			return static_cast<const ComponentStorage<ComponentType>*>(iterator->second.get());
		}

		[[nodiscard]] EntityUuid GenerateUuid();

	private:
		std::vector<EntityRecord> m_Records;
		std::vector<uint32_t> m_FreeIndices;
		std::unordered_map<EntityUuid, EntityId> m_UuidToEntity;
		std::unordered_map<ComponentTypeId, std::unique_ptr<IComponentStorage>> m_ComponentStorages;
		size_t m_EntityCount = 0;
		uint64_t m_NextUuid = 1;

		template<typename... Terms>
		friend class Query;
	};
}

#include "HuaEngine/ECS/Query.h"

namespace HE {
	template<typename... Terms>
	HE::Query<Terms...> World::Query() {
		return HE::Query<Terms...>(*this);
	}

	template<typename T>
	T* Entity::TryGetComponent() {
		return m_World != nullptr ? m_World->TryGetComponent<T>(m_Id) : nullptr;
	}

	template<typename T>
	const T* Entity::TryGetComponent() const {
		return m_World != nullptr ? m_World->TryGetComponent<T>(m_Id) : nullptr;
	}
}
