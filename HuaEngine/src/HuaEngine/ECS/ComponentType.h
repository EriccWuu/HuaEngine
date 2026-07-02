#pragma once

#include <atomic>
#include <cstdint>
#include <type_traits>

namespace HE {
	using ComponentTypeId = uint32_t;

	inline constexpr ComponentTypeId InvalidComponentTypeId = 0;

	namespace Detail {
		inline ComponentTypeId NextComponentTypeId() {
			static std::atomic<ComponentTypeId> nextTypeId{InvalidComponentTypeId + 1};
			return nextTypeId.fetch_add(1, std::memory_order_relaxed);
		}

		template<typename T>
		using CleanComponentType = std::remove_cv_t<std::remove_reference_t<T>>;

		template<typename T>
		struct ComponentTypeIdHolder {
			static ComponentTypeId Value() {
				static const ComponentTypeId typeId = NextComponentTypeId();
				return typeId;
			}
		};
	}

	template<typename T>
	ComponentTypeId ComponentTypeIdOf() {
		using ComponentType = Detail::CleanComponentType<T>;
		return Detail::ComponentTypeIdHolder<ComponentType>::Value();
	}

	template<typename T>
	struct Read {
		using ComponentType = Detail::CleanComponentType<T>;
	};

	template<typename T>
	struct QueryTermTraits {
		using ComponentType = Detail::CleanComponentType<T>;
		static constexpr bool IsReadOnly = false;
	};

	template<typename T>
	struct QueryTermTraits<Read<T>> {
		using ComponentType = typename Read<T>::ComponentType;
		static constexpr bool IsReadOnly = true;
	};

	template<typename T>
	struct IsReadTerm {
		static constexpr bool Value = false;
	};

	template<typename T>
	struct IsReadTerm<Read<T>> {
		static constexpr bool Value = true;
	};
}
