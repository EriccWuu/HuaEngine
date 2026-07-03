#include "GeneratedReflection.h"

#include "HuaEngine/ECS/ComponentRegistry.h"
#include "HuaEngine/ECS/Components.h"
#include "Module/Rendering/RenderingComponent.h"

namespace HE::Generated {

static constexpr ReflectedFieldInfo Type0Fields[] = {
    {"Name", "std::string"},
};

static constexpr ReflectedFieldInfo Type1Fields[] = {
    {"Primary", "bool"},
    {"FixedAspectRatio", "bool"},
};

static constexpr ReflectedFieldInfo Type2Fields[] = {
    {"MaterialInstance", "Ref<HE::Rendering::MaterialInstance>"},
};

static constexpr ReflectedFieldInfo Type3Fields[] = {
    {"MeshAssetName", "std::string"},
};

static constexpr ReflectedFieldInfo Type4Fields[] = {
    {"Position", "glm::vec3"},
    {"Rotation", "glm::vec3"},
    {"Scale", "glm::vec3"},
};

static constexpr ReflectedTypeInfo Types[] = {
    {"NameComponent", "HE::NameComponent", "component", "Name", "Core", std::span<const ReflectedFieldInfo>{Type0Fields}},
    {"CameraComponent", "HE::Rendering::CameraComponent", "component", "Camera", "Rendering", std::span<const ReflectedFieldInfo>{Type1Fields}},
    {"MaterialComponent", "HE::Rendering::MaterialComponent", "component", "Material", "Rendering", std::span<const ReflectedFieldInfo>{Type2Fields}},
    {"MeshComponent", "HE::Rendering::MeshComponent", "component", "Mesh", "Rendering", std::span<const ReflectedFieldInfo>{Type3Fields}},
    {"TransformComponent", "HE::TransformComponent", "component", "Transform", "Core", std::span<const ReflectedFieldInfo>{Type4Fields}},
};

std::span<const ReflectedTypeInfo> GetReflectedTypes() {
    return Types;
}

const ReflectedTypeInfo* FindReflectedType(std::string_view qualifiedName) {
    for (const ReflectedTypeInfo& type : GetReflectedTypes()) {
        if (type.QualifiedName == qualifiedName) {
            return &type;
        }
    }

    return nullptr;
}

void RegisterGeneratedComponents(ComponentRegistry& registry) {
    registry.Register<HE::NameComponent>({
        .TypeName = "NameComponent",
        .DisplayName = "Name",
        .Category = "Core"
    });
    registry.Register<HE::Rendering::CameraComponent>({
        .TypeName = "CameraComponent",
        .DisplayName = "Camera",
        .Category = "Rendering"
    });
    registry.Register<HE::Rendering::MaterialComponent>({
        .TypeName = "MaterialComponent",
        .DisplayName = "Material",
        .Category = "Rendering"
    });
    registry.Register<HE::Rendering::MeshComponent>({
        .TypeName = "MeshComponent",
        .DisplayName = "Mesh",
        .Category = "Rendering"
    });
    registry.Register<HE::TransformComponent>({
        .TypeName = "TransformComponent",
        .DisplayName = "Transform",
        .Category = "Core"
    });
}

} // namespace HE::Generated
