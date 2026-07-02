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
			using FirstTerm = std::tuple_element_t<0, std::tuple<Terms...>>;
			using FirstComponent = typename QueryTermTraits<FirstTerm>::ComponentType;

			auto* primaryStorage = m_World->template GetStorage<FirstComponent>();
			if (primaryStorage == nullptr) {
				return;
			}

			for (auto& [entityId, component] : primaryStorage->Components) {
				(void)component;
				if (!m_World->IsAlive(entityId)) {
					continue;
				}

				if (((m_World->template TryGetComponent<typename QueryTermTraits<Terms>::ComponentType>(entityId) != nullptr) && ...)) {
					std::invoke(std::forward<Callback>(callback), entityId, GetArgument<Terms>(entityId)...);
				}
			}
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
