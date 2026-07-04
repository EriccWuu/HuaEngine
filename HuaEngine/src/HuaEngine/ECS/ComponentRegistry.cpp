#include "HuaEngine/ECS/ComponentRegistry.h"
#include "HuaEngine/Generated/GeneratedReflection.h"
#include "HuaEngine/Reflection/Reflection.h"

namespace HE {
	bool ComponentRegistry::Register(const Refl::RuntimeTypeDescriptor& descriptor) {
		if (descriptor.TypeId == InvalidComponentTypeId ||
			descriptor.Name.empty() ||
			descriptor.Size == 0 ||
			m_ByTypeId.find(descriptor.TypeId) != m_ByTypeId.end() ||
			m_ByName.find(std::string(descriptor.Name)) != m_ByName.end()) {
			return false;
		}

		ComponentMetadata metadata;
		metadata.TypeId = descriptor.TypeId;
		metadata.RuntimeType = &descriptor;
		metadata.TypeName = descriptor.Name;
		metadata.DisplayName = descriptor.DisplayName;
		metadata.Category = descriptor.Category;
		metadata.Size = descriptor.Size;
		metadata.AllowMultiple = false;
		metadata.ConstructDefault = descriptor.ConstructDefault;
		metadata.Destroy = descriptor.Destroy;
		metadata.Copy = descriptor.Copy;
		metadata.AddCopyToWorld = descriptor.AddCopyToWorld;

		const size_t index = m_Metadata.size();
		m_ByTypeId.emplace(metadata.TypeId, index);
		m_ByName.emplace(metadata.TypeName, index);
		m_Metadata.emplace_back(std::move(metadata));
		return true;
	}

	const ComponentMetadata* ComponentRegistry::FindByTypeId(ComponentTypeId typeId) const {
		const auto it = m_ByTypeId.find(typeId);
		if (it == m_ByTypeId.end()) {
			return nullptr;
		}

		return &m_Metadata[it->second];
	}

	const ComponentMetadata* ComponentRegistry::FindByName(std::string_view typeName) const {
		for (const ComponentMetadata& metadata : m_Metadata) {
			if (metadata.TypeName == typeName) {
				return &metadata;
			}
		}

		return nullptr;
	}

	const std::vector<ComponentMetadata>& ComponentRegistry::GetAll() const {
		return m_Metadata;
	}

	void RegisterCoreComponents(ComponentRegistry& registry) {
		Generated::RegisterGeneratedComponents(registry);
	}
}
