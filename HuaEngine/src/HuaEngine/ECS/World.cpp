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
		uint32_t index = 0;

		if (!m_FreeIndices.empty()) {
			index = m_FreeIndices.back();
			m_FreeIndices.pop_back();

			EntityRecord& record = m_Records[index];
			++record.Generation;
			record.Alive = true;
			record.Uuid = GenerateUuid();
			record.Name = name.empty() ? "Entity" : name;
		}
		else {
			index = static_cast<uint32_t>(m_Records.size());
			EntityRecord record;
			record.Generation = 1;
			record.Alive = true;
			record.Uuid = GenerateUuid();
			record.Name = name.empty() ? "Entity" : name;
			m_Records.push_back(std::move(record));
		}

		const EntityId id{index, m_Records[index].Generation};
		m_UuidToEntity[m_Records[index].Uuid] = id;
		++m_EntityCount;

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

	EntityUuid World::GenerateUuid() {
		return EntityUuid{0, m_NextUuid++};
	}
}
