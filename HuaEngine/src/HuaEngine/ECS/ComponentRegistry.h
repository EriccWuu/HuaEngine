#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "HuaEngine/ECS/ComponentType.h"
#include "HuaEngine/ECS/EntityId.h"
#include "HuaEngine/ECS/World.h"
#include "HuaEngine/Serialization/SerializationCore.h"

namespace HE {
	namespace Refl {
		struct RuntimeTypeDescriptor;
	}

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
		std::function<void(Serialization::SerializationBackend&, const std::string&, const void*)> Serialize;
		std::function<bool(Serialization::SerializationBackend&, const std::string&, void*)> Deserialize;
		std::function<void(World&, EntityId, const void*)> AddCopyToWorld;
	};

	class ComponentRegistry {
	public:
		template<typename T>
		bool Register(ComponentRegistration registration) {
			using ComponentType = Detail::CleanComponentType<T>;

			const ComponentTypeId typeId = ComponentTypeIdOf<ComponentType>();
			if (m_ByTypeId.find(typeId) != m_ByTypeId.end() ||
				m_ByName.find(registration.TypeName) != m_ByName.end()) {
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
			metadata.Serialize = [](Serialization::SerializationBackend& backend, const std::string& name, const void* component) {
				Serialization::Serializer<ComponentType>::Serialize(backend, name, *static_cast<const ComponentType*>(component));
			};
			metadata.Deserialize = [](Serialization::SerializationBackend& backend, const std::string& name, void* component) {
				return Serialization::Serializer<ComponentType>::Deserialize(backend, name, *static_cast<ComponentType*>(component));
			};
			metadata.AddCopyToWorld = [](World& world, EntityId id, const void* component) {
				world.AddComponent<ComponentType>(id, *static_cast<const ComponentType*>(component));
			};

			const size_t index = m_Metadata.size();
			m_ByTypeId.emplace(metadata.TypeId, index);
			m_ByName.emplace(metadata.TypeName, index);
			m_Metadata.emplace_back(std::move(metadata));
			return true;
		}

		bool Register(const Refl::RuntimeTypeDescriptor& descriptor);

		template<typename T>
		[[nodiscard]] const ComponentMetadata* FindByType() const {
			return FindByTypeId(ComponentTypeIdOf<T>());
		}

		[[nodiscard]] const ComponentMetadata* FindByTypeId(ComponentTypeId typeId) const;
		[[nodiscard]] const ComponentMetadata* FindByName(std::string_view typeName) const;
		[[nodiscard]] const std::vector<ComponentMetadata>& GetAll() const;

	private:
		std::vector<ComponentMetadata> m_Metadata;
		std::unordered_map<ComponentTypeId, size_t> m_ByTypeId;
		std::unordered_map<std::string, size_t> m_ByName;
	};

	void RegisterCoreComponents(ComponentRegistry& registry);
}
