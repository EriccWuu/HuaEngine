#include "enginepch.h"
#include "HuaEngine/ECS/World.h"

namespace HE {
	namespace Detail {
		EntityUuid GetWorldEntityUuid(const World* world, EntityId id) {
			return world != nullptr ? world->GetUuid(id) : EntityUuid{};
		}

		std::string GetWorldEntityName(const World* world, EntityId id) {
			return world != nullptr ? world->GetEntityName(id) : "Entity";
		}

		void SetWorldEntityName(World* world, EntityId id, const std::string& name) {
			if (world != nullptr) {
				world->SetEntityName(id, name);
			}
		}

		bool IsWorldEntityAlive(const World* world, EntityId id) {
			return world != nullptr && world->IsAlive(id);
		}
	}

	Entity World::CreateEntity(const std::string& name) {
		return CreateEntityWithUuid(GenerateUuid(), name);
	}

	Entity World::CreateEntityWithUuid(EntityUuid uuid, std::string_view name) {
		if (uuid == EntityUuid{}) {
			uuid = GenerateUuid();
		}

		Entity existingEntity = GetEntity(uuid);
		if (existingEntity.IsValid()) {
			return existingEntity;
		}

		uint32_t index = 0;

		if (!m_FreeIndices.empty()) {
			index = m_FreeIndices.back();
			m_FreeIndices.pop_back();

			EntityRecord& record = m_Records[index];
			++record.Generation;
			record.Alive = true;
			record.Uuid = uuid;
			record.Name = name.empty() ? "Entity" : std::string(name);
		}
		else {
			index = static_cast<uint32_t>(m_Records.size());
			EntityRecord record;
			record.Generation = 1;
			record.Alive = true;
			record.Uuid = uuid;
			record.Name = name.empty() ? "Entity" : std::string(name);
			m_Records.push_back(std::move(record));
		}

		const EntityId id{index, m_Records[index].Generation};
		m_UuidToEntity[m_Records[index].Uuid] = id;
		++m_EntityCount;

		if (uuid.High == 0 && uuid.Low >= m_NextUuid) {
			m_NextUuid = uuid.Low + 1;
		}

		return Entity(id, this);
	}

	void World::DestroyEntity(EntityId id) {
		if (!IsAlive(id)) {
			return;
		}

		EntityRecord& record = m_Records[id.Index];
		record.Alive = false;
		m_UuidToEntity.erase(record.Uuid);
		m_FreeIndices.push_back(id.Index);
		--m_EntityCount;

		for (auto& [typeId, storage] : m_ComponentStorages) {
			(void)typeId;
			storage->Remove(id);
		}
	}

	void World::Clear() {
		m_Records.clear();
		m_FreeIndices.clear();
		m_UuidToEntity.clear();
		m_ComponentStorages.clear();
		m_EntityCount = 0;
		m_NextUuid = 1;
	}

	bool World::IsAlive(EntityId id) const {
		return id.Generation != 0
			&& id.Index < m_Records.size()
			&& m_Records[id.Index].Alive
			&& m_Records[id.Index].Generation == id.Generation;
	}

	EntityId World::FindEntity(EntityUuid uuid) const {
		auto iterator = m_UuidToEntity.find(uuid);
		if (iterator == m_UuidToEntity.end()) {
			return {};
		}

		return iterator->second;
	}

	Entity World::GetEntity(EntityId id) {
		if (!IsAlive(id)) {
			return {};
		}

		return Entity(id, this);
	}

	Entity World::GetEntity(EntityUuid uuid) {
		return GetEntity(FindEntity(uuid));
	}

	Entity World::GetEntityByIndex(uint32_t index) {
		if (index >= m_Records.size()) {
			return {};
		}

		const EntityId id{index, m_Records[index].Generation};
		return GetEntity(id);
	}

	EntityUuid World::GetUuid(EntityId id) const {
		if (!IsAlive(id)) {
			return {};
		}

		return m_Records[id.Index].Uuid;
	}

	std::string World::GetEntityName(EntityId id) const {
		if (!IsAlive(id)) {
			return "Entity";
		}

		return m_Records[id.Index].Name;
	}

	void World::SetEntityName(EntityId id, const std::string& name) {
		if (!IsAlive(id)) {
			return;
		}

		m_Records[id.Index].Name = name.empty() ? "Entity" : name;
	}

	const void* World::TryGetComponentByType(EntityId id, ComponentTypeId typeId) const {
		if (!IsAlive(id)) {
			return nullptr;
		}

		const auto iterator = m_ComponentStorages.find(typeId);
		if (iterator == m_ComponentStorages.end()) {
			return nullptr;
		}

		return iterator->second->TryGetRaw(id);
	}

	void* World::TryGetComponentByType(EntityId id, ComponentTypeId typeId) {
		if (!IsAlive(id)) {
			return nullptr;
		}

		const auto iterator = m_ComponentStorages.find(typeId);
		if (iterator == m_ComponentStorages.end()) {
			return nullptr;
		}

		return iterator->second->TryGetRaw(id);
	}

	std::vector<ComponentTypeId> World::ListComponentTypes(EntityId id) const {
		std::vector<ComponentTypeId> componentTypes;
		if (!IsAlive(id)) {
			return componentTypes;
		}

		for (const auto& [typeId, storage] : m_ComponentStorages) {
			if (storage->Contains(id)) {
				componentTypes.push_back(typeId);
			}
		}

		return componentTypes;
	}

	EntityUuid World::GenerateUuid() {
		return EntityUuid{0, m_NextUuid++};
	}
}
