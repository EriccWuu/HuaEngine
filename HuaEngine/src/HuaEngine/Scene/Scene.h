#pragma once

#include <string>
#include "HuaEngine/ECS/Entity.h"
#include "HuaEngine/ECS/EntityManager.h"
#include "HuaEngine/ECS/Scheduler.h"
#include "HuaEngine/ECS/System.h"
#include "HuaEngine/ECS/World.h"

namespace HE {

	class Scene {
	public:
		Scene() = default;
		Scene(const std::string& name) : m_Name(name) {}
		~Scene() = default;

		void Update();
		void OnRuntimeStart();
		void OnUpdate(float deltaTime = 0.0f);
		void OnRuntimeStop();

		// Scene name
		const std::string& GetName() const { return m_Name; }
		void SetName(const std::string& name) { m_Name = name; }

		void AddSystem(Ref<System> system);
		void AddSyetem(Ref<System> system) { AddSystem(system); }
		World& GetWorld() { return m_World; }
		const World& GetWorld() const { return m_World; }
		Scheduler& GetScheduler() { return m_Scheduler; }
		EntityManager& GetEntityManager() { return m_EntityManager; }

		template<typename Type, typename... Other, typename... Args>
		[[nodiscard]] decltype(auto) View(Args... args) {
			return m_EntityManager.GetRegistry().view<Type, Other...>(args...);
		}

		template<typename... Type, typename... Args>
		[[nodiscard]] decltype(auto) Get(Args... args) {
			return m_EntityManager.GetRegistry().get<Type...>(args...);
		}

		template<typename T>
		[[nodiscard]] Ref<T> FindSystem() const {
			for (const auto& system : m_Systems) {
				auto typedSystem = std::dynamic_pointer_cast<T>(system);
				if (typedSystem) {
					return typedSystem;
				}
			}

			return nullptr;
		}

	private:
		std::string m_Name;
		World m_World;
		Scheduler m_Scheduler;
		EntityManager m_EntityManager;
		std::vector<Ref<System>> m_Systems;
	};
}
