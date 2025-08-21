#pragma once 

#include "entt.hpp"

namespace HE {
	class Entity;

	class EntityManager {
	public:
		Entity CreateEntity(const std::string& name = "Entity");
		void DestroyEntity(Entity entity);
		entt::registry& GetRegistry() { return m_Registry; }

	private:
		entt::registry m_Registry;

		friend class Entity;
	};
}