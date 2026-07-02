#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "HuaEngine/ECS/EntityId.h"
#include "HuaEngine/ECS/World.h"

namespace HE {
	class CommandBuffer {
	public:
		EntityUuid CreateEntity(std::string name = "Entity") {
			const EntityUuid uuid = AllocateDeferredUuid();
			m_Commands.emplace_back([uuid, name = std::move(name)](World& world) {
				world.CreateEntityWithUuid(uuid, name);
			});
			return uuid;
		}

		void DestroyEntity(EntityId id) {
			m_Commands.emplace_back([id](World& world) {
				world.DestroyEntity(id);
			});
		}

		template<typename T>
		void AddComponent(EntityId id, T component) {
			m_Commands.emplace_back([id, component = std::move(component)](World& world) mutable {
				world.AddComponent<T>(id, std::move(component));
			});
		}

		template<typename T>
		void RemoveComponent(EntityId id) {
			m_Commands.emplace_back([id](World& world) {
				world.RemoveComponent<T>(id);
			});
		}

		void Playback(World& world) {
			auto commands = std::move(m_Commands);
			m_Commands.clear();
			for (auto& command : commands) {
				command(world);
			}
		}

		void Clear() {
			m_Commands.clear();
		}

		[[nodiscard]] bool IsEmpty() const {
			return m_Commands.empty();
		}

	private:
		EntityUuid AllocateDeferredUuid() {
			constexpr uint64_t deferredUuidNamespace = 0x4845436d64427566ULL;
			return EntityUuid{deferredUuidNamespace, m_NextDeferredUuid++};
		}

	private:
		std::vector<std::function<void(World&)>> m_Commands;
		uint64_t m_NextDeferredUuid = 1;
	};
}
