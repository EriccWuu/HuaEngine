#pragma once

#include <functional>
#include <tuple>
#include <type_traits>

#include "HuaEngine/ECS/ComponentType.h"
#include "HuaEngine/ECS/EntityId.h"

namespace HE {
	class World;

	template<typename... Terms>
	class Query {
	public:
		explicit Query(World& world)
			: m_World(&world) {
			static_assert(sizeof...(Terms) > 0, "Query requires at least one component term");
		}

		template<typename Callback>
		void ForEach(Callback&& callback) {
			m_World->ForEachEntity([&](auto entity) {
				const EntityId entityId = entity.GetId();
				if (((m_World->template TryGetComponent<typename QueryTermTraits<Terms>::ComponentType>(entityId) != nullptr) && ...)) {
					std::invoke(std::forward<Callback>(callback), entity, GetArgument<Terms>(entityId)...);
				}
			});
		}

	private:
		template<typename Term>
		decltype(auto) GetArgument(EntityId entityId) {
			using ComponentType = typename QueryTermTraits<Term>::ComponentType;
			ComponentType* component = m_World->template TryGetComponent<ComponentType>(entityId);

			if constexpr (QueryTermTraits<Term>::IsReadOnly) {
				return static_cast<const ComponentType&>(*component);
			}
			else {
				return static_cast<ComponentType&>(*component);
			}
		}

	private:
		World* m_World = nullptr;
	};
}
