#include "HuaEngine/ECS/ComponentRegistry.h"
#include "HuaEngine/ECS/Components.h"
#include "Module/Rendering/RenderingComponent.h"

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

	void RegisterCoreComponents(ComponentRegistry& registry) {
		registry.Register<NameComponent>({
			.TypeName = "NameComponent",
			.DisplayName = "Name",
			.Category = "Core"
		});
		registry.Register<TransformComponent>({
			.TypeName = "TransformComponent",
			.DisplayName = "Transform",
			.Category = "Core"
		});
		registry.Register<Rendering::CameraComponent>({
			.TypeName = "CameraComponent",
			.DisplayName = "Camera",
			.Category = "Rendering"
		});
		registry.Register<Rendering::MeshComponent>({
			.TypeName = "MeshComponent",
			.DisplayName = "Mesh",
			.Category = "Rendering"
		});
		registry.Register<Rendering::MaterialComponent>({
			.TypeName = "MaterialComponent",
			.DisplayName = "Material",
			.Category = "Rendering"
		});
	}
}
