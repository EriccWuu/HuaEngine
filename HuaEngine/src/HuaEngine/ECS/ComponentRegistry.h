#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "HuaEngine/ECS/ComponentType.h"

namespace HE {
	struct ComponentRegistration {
		std::string TypeName;
		std::string DisplayName;
		std::string Category;
		bool AllowMultiple = false;
	};

	struct ComponentMetadata {
		ComponentTypeId TypeId = InvalidComponentTypeId;
		std::string TypeName;
		std::string DisplayName;
		std::string Category;
		size_t Size = 0;
		bool AllowMultiple = false;
		void* (*ConstructDefault)() = nullptr;
		void (*Destroy)(void*) = nullptr;
		void* (*Copy)(const void*) = nullptr;
	};

	class ComponentRegistry {
	public:
		template<typename T>
		bool Register(ComponentRegistration registration) {
			using ComponentType = Detail::CleanComponentType<T>;

			const ComponentTypeId typeId = ComponentTypeIdOf<ComponentType>();
			if (m_TypeIdToIndex.find(typeId) != m_TypeIdToIndex.end() ||
				m_TypeNameToIndex.find(registration.TypeName) != m_TypeNameToIndex.end()) {
				return false;
			}

			ComponentMetadata metadata;
			metadata.TypeId = typeId;
			metadata.TypeName = std::move(registration.TypeName);
			metadata.DisplayName = std::move(registration.DisplayName);
			metadata.Category = std::move(registration.Category);
			metadata.Size = sizeof(ComponentType);
			metadata.AllowMultiple = registration.AllowMultiple;
			metadata.ConstructDefault = []() -> void* {
				return new ComponentType();
			};
			metadata.Destroy = [](void* instance) {
				delete static_cast<ComponentType*>(instance);
			};
			metadata.Copy = [](const void* instance) -> void* {
				return new ComponentType(*static_cast<const ComponentType*>(instance));
			};

			const size_t index = m_Components.size();
			m_TypeIdToIndex.emplace(metadata.TypeId, index);
			m_TypeNameToIndex.emplace(metadata.TypeName, index);
			m_Components.emplace_back(std::move(metadata));
			return true;
		}

		template<typename T>
		[[nodiscard]] const ComponentMetadata* FindByType() const {
			return FindByTypeId(ComponentTypeIdOf<T>());
		}

		[[nodiscard]] const ComponentMetadata* FindByTypeId(ComponentTypeId typeId) const;
		[[nodiscard]] const ComponentMetadata* FindByName(const std::string& typeName) const;
		[[nodiscard]] const std::vector<ComponentMetadata>& GetAll() const;

	private:
		std::vector<ComponentMetadata> m_Components;
		std::unordered_map<ComponentTypeId, size_t> m_TypeIdToIndex;
		std::unordered_map<std::string, size_t> m_TypeNameToIndex;
	};

	inline const ComponentMetadata* ComponentRegistry::FindByTypeId(ComponentTypeId typeId) const {
		const auto it = m_TypeIdToIndex.find(typeId);
		if (it == m_TypeIdToIndex.end()) {
			return nullptr;
		}

		return &m_Components[it->second];
	}

	inline const ComponentMetadata* ComponentRegistry::FindByName(const std::string& typeName) const {
		const auto it = m_TypeNameToIndex.find(typeName);
		if (it == m_TypeNameToIndex.end()) {
			return nullptr;
		}

		return &m_Components[it->second];
	}

	inline const std::vector<ComponentMetadata>& ComponentRegistry::GetAll() const {
		return m_Components;
	}
}
