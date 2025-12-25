#pragma once

#include <string>
#include "HuaEngine/ECS/Entity.h"
#include "HuaEngine/ECS/Syetem.h"
#include "HuaEngine/ECS/EntityManager.h"

namespace HE {

	class Scene {
	public:
		Scene() = default;
		Scene(const std::string& name) : m_Name(name) {}
		~Scene() = default;

		void Update();

		// Scene name
		const std::string& GetName() const { return m_Name; }
		void SetName(const std::string& name) { m_Name = name; }

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
		std::string m_Name;
		EntityManager m_EntityManager;
		std::vector<Ref<System>> m_Systems;
	};
}