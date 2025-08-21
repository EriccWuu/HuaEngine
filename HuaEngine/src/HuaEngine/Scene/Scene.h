#pragma once

#include "HuaEngine/ECS/Entity.h"
#include "HuaEngine/ECS/Syetem.h"
#include "HuaEngine/ECS/EntityManager.h"

namespace HE {

	class Scene {
	public:
		Scene() = default;
		~Scene() = default;

		void Update();

		void AddSyetem(Ref<System> system) { m_Systems.emplace_back(system); }
		EntityManager& GetEntityManager() { return m_EntityManager; }

		template<typename Type, typename... Other, typename... Args>
		[[nodiscard]] decltype(auto) View(Args... args) {
			return m_EntityManager.GetRegistry().view<Type, Other...>(args...);
		}

		template<typename... Type, typename... Args>
		[[nodiscard]] decltype(auto) Get(Args... args) {
			return m_EntityManager.GetRegistry().get<Type...>(args...);
		}

	private:
		EntityManager m_EntityManager;
		std::vector<Ref<System>> m_Systems;
	};
}