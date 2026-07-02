#include "enginepch.h"
#include "HuaEngine/ECS/World.h"

#include <algorithm>

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
			record.RawEntity = m_Registry.create();
		}
		else {
			index = static_cast<uint32_t>(m_Records.size());
			EntityRecord record;
			record.Generation = 1;
			record.Alive = true;
			record.Uuid = uuid;
			record.Name = name.empty() ? "Entity" : std::string(name);
			record.RawEntity = m_Registry.create();
			m_Records.push_back(std::move(record));
		}

		const EntityId id{index, m_Records[index].Generation};
		m_UuidToEntity[m_Records[index].Uuid] = id;
		++m_EntityCount;

		if (uuid.High == 0 && uuid.Low >= m_NextUuid) {
			m_NextUuid = uuid.Low + 1;
		}

		auto entity = Entity(id, this);
		entity.AddComponent<TransformComponent>();
		return entity;
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
		m_ComponentTypesByEntity.erase(id);

		if (record.RawEntity != entt::null && m_Registry.valid(record.RawEntity)) {
			m_Registry.destroy(record.RawEntity);
		}
		record.RawEntity = entt::null;
	}

	void World::Clear() {
		m_Records.clear();
		m_FreeIndices.clear();
		m_UuidToEntity.clear();
		m_ComponentTypesByEntity.clear();
		m_ComponentAccessors.clear();
		m_Registry.clear();
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
		const auto* record = TryGetRecord(id);
		if (record == nullptr) {
			return nullptr;
		}

		const auto iterator = m_ComponentAccessors.find(typeId);
		if (iterator == m_ComponentAccessors.end() || iterator->second.TryGetRawConst == nullptr) {
			return nullptr;
		}

		return iterator->second.TryGetRawConst(m_Registry, record->RawEntity);
	}

	void* World::TryGetComponentByType(EntityId id, ComponentTypeId typeId) {
		auto* record = TryGetRecord(id);
		if (record == nullptr) {
			return nullptr;
		}

		const auto iterator = m_ComponentAccessors.find(typeId);
		if (iterator == m_ComponentAccessors.end() || iterator->second.TryGetRaw == nullptr) {
			return nullptr;
		}

		return iterator->second.TryGetRaw(m_Registry, record->RawEntity);
	}

	std::vector<ComponentTypeId> World::ListComponentTypes(EntityId id) const {
		const auto iterator = m_ComponentTypesByEntity.find(id);
		if (iterator == m_ComponentTypesByEntity.end()) {
			return {};
		}

		return iterator->second;
	}

	EntityUuid World::GenerateUuid() {
		return EntityUuid{0, m_NextUuid++};
	}

	World::EntityRecord* World::TryGetRecord(EntityId id) {
		if (!IsAlive(id)) {
			return nullptr;
		}

		return &m_Records[id.Index];
	}

	const World::EntityRecord* World::TryGetRecord(EntityId id) const {
		if (!IsAlive(id)) {
			return nullptr;
		}

		return &m_Records[id.Index];
	}

	void World::TrackComponentType(EntityId id, ComponentTypeId typeId) {
		auto& componentTypes = m_ComponentTypesByEntity[id];
		if (std::find(componentTypes.begin(), componentTypes.end(), typeId) == componentTypes.end()) {
			componentTypes.push_back(typeId);
		}
	}

	void World::UntrackComponentType(EntityId id, ComponentTypeId typeId) {
		auto iterator = m_ComponentTypesByEntity.find(id);
		if (iterator == m_ComponentTypesByEntity.end()) {
			return;
		}

		auto& componentTypes = iterator->second;
		componentTypes.erase(std::remove(componentTypes.begin(), componentTypes.end(), typeId), componentTypes.end());
		if (componentTypes.empty()) {
			m_ComponentTypesByEntity.erase(iterator);
		}
	}
}
