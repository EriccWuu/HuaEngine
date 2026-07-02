#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
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
		Entity CreateEntityWithUuid(EntityUuid uuid, std::string_view name);
		void DestroyEntity(EntityId id);
		void Clear();

		[[nodiscard]] bool IsAlive(EntityId id) const;
		[[nodiscard]] size_t GetEntityCount() const { return m_EntityCount; }
		[[nodiscard]] EntityId FindEntity(EntityUuid uuid) const;
		[[nodiscard]] Entity GetEntity(EntityId id);
		[[nodiscard]] Entity GetEntity(EntityUuid uuid);
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
			virtual void Remove(EntityId id) = 0;
			virtual bool Contains(EntityId id) const = 0;
			virtual void* TryGetRaw(EntityId id) = 0;
			virtual const void* TryGetRaw(EntityId id) const = 0;
		};

		template<typename T>
		struct ComponentStorage final : IComponentStorage {
			std::unordered_map<EntityId, T> Components;

			void Remove(EntityId id) override {
				Components.erase(id);
			}

			bool Contains(EntityId id) const override {
				return Components.find(id) != Components.end();
			}

			void* TryGetRaw(EntityId id) override {
				auto iterator = Components.find(id);
				return iterator != Components.end() ? &iterator->second : nullptr;
			}

			const void* TryGetRaw(EntityId id) const override {
				auto iterator = Components.find(id);
				return iterator != Components.end() ? &iterator->second : nullptr;
			}
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

	template<typename T, typename... Args>
	T& Entity::AddComponent(Args&&... args) {
		if (m_World != nullptr) {
			return m_World->AddComponent<T>(m_Id, std::forward<Args>(args)...);
		}

		T& component = m_EntityManager->m_Registry.template emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
		return component;
	}

	template<typename T>
	T& Entity::GetComponent() {
		if (m_World != nullptr) {
			return *m_World->TryGetComponent<T>(m_Id);
		}

		T& component = m_EntityManager->m_Registry.template get<T>(m_EntityHandle);
		return component;
	}

	template<typename T>
	const T& Entity::GetComponent() const {
		if (m_World != nullptr) {
			return *m_World->TryGetComponent<T>(m_Id);
		}

		const T& component = m_EntityManager->m_Registry.template get<T>(m_EntityHandle);
		return component;
	}

	template<typename T>
	bool Entity::HasComponent() const {
		if (m_World != nullptr) {
			return m_World->HasComponent<T>(m_Id);
		}

		return m_EntityManager != nullptr
			&& m_EntityManager->m_Registry.template all_of<T>(m_EntityHandle);
	}

	template<typename T>
	void Entity::RemoveComponent() {
		if (m_World != nullptr) {
			m_World->RemoveComponent<T>(m_Id);
			return;
		}

		m_EntityManager->m_Registry.template remove<T>(m_EntityHandle);
	}

	template<typename T>
	T* Entity::TryGetComponent() {
		if (m_World != nullptr) {
			return m_World->TryGetComponent<T>(m_Id);
		}

		if (m_EntityManager == nullptr || !m_EntityManager->m_Registry.template all_of<T>(m_EntityHandle)) {
			return nullptr;
		}

		return &m_EntityManager->m_Registry.template get<T>(m_EntityHandle);
	}

	template<typename T>
	const T* Entity::TryGetComponent() const {
		if (m_World != nullptr) {
			return m_World->TryGetComponent<T>(m_Id);
		}

		if (m_EntityManager == nullptr || !m_EntityManager->m_Registry.template all_of<T>(m_EntityHandle)) {
			return nullptr;
		}

		return &m_EntityManager->m_Registry.template get<T>(m_EntityHandle);
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
