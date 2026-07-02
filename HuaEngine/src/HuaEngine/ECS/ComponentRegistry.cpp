#include "HuaEngine/ECS/ComponentRegistry.h"

namespace HE {
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
}
