#pragma once

#include <any>
#include <typeindex>
#include <unordered_map>

namespace HE {
	class FrameContext {
	public:
		void BeginFrame() {
			m_Resources.clear();
		}

		template<typename T>
		T& GetOrCreate() {
			const auto [iterator, inserted] = m_Resources.try_emplace(std::type_index(typeid(T)), T{});
			return std::any_cast<T&>(iterator->second);
		}

		template<typename T>
		T* TryGet() {
			const auto iterator = m_Resources.find(std::type_index(typeid(T)));
			return iterator == m_Resources.end() ? nullptr : std::any_cast<T>(&iterator->second);
		}

		template<typename T>
		const T* TryGet() const {
			const auto iterator = m_Resources.find(std::type_index(typeid(T)));
			return iterator == m_Resources.end() ? nullptr : std::any_cast<T>(&iterator->second);
		}

	private:
		std::unordered_map<std::type_index, std::any> m_Resources;
	};
}
