#include "enginepch.h"
#include "HuaEngine/ECS/Components.h"
#include "EntityManager.h"
#include "Entity.h"

namespace HE {
	Entity EntityManager::CreateEntity(const std::string& name) {
		Entity entity = { m_Registry.create(), this };
		entity.AddComponent<NameComponent>(name.empty() ? "Entity" : name);
		entity.AddComponent<TransformComponent>();
		return entity;
	}

	void EntityManager::DestroyEntity(Entity entity) {
		m_Registry.destroy(entity);
	}
}
