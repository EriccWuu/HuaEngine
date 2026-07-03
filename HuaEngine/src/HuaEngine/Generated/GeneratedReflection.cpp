#include "GeneratedReflection.h"

namespace HE::Reflection::Generated {

static constexpr GeneratedFieldInfo Type0Fields[] = {
    {"Name", "std::string", "", ""},
};

static constexpr GeneratedFieldInfo Type1Fields[] = {
    {"Primary", "bool", "", ""},
    {"FixedAspectRatio", "bool", "", ""},
};

static constexpr GeneratedFieldInfo Type2Fields[] = {
    {"MaterialInstance", "Ref<HE::Rendering::MaterialInstance>", "", ""},
};

static constexpr GeneratedFieldInfo Type3Fields[] = {
    {"MeshAssetName", "std::string", "", ""},
};

static constexpr GeneratedFieldInfo Type4Fields[] = {
    {"Position", "glm::vec3", "", ""},
    {"Rotation", "glm::vec3", "", ""},
    {"Scale", "glm::vec3", "", ""},
};

static constexpr GeneratedTypeInfo Types[] = {
    {"NameComponent", "HE::NameComponent", "Name", "Core", "HuaEngine/src/HuaEngine/ECS/Components.h", Type0Fields, 1},
    {"CameraComponent", "HE::Rendering::CameraComponent", "Camera", "Rendering", "HuaEngine/src/Module/Rendering/RenderingComponent.h", Type1Fields, 2},
    {"MaterialComponent", "HE::Rendering::MaterialComponent", "Material", "Rendering", "HuaEngine/src/Module/Rendering/RenderingComponent.h", Type2Fields, 1},
    {"MeshComponent", "HE::Rendering::MeshComponent", "Mesh", "Rendering", "HuaEngine/src/Module/Rendering/RenderingComponent.h", Type3Fields, 1},
    {"TransformComponent", "HE::TransformComponent", "Transform", "Core", "HuaEngine/src/HuaEngine/ECS/Components.h", Type4Fields, 3},
};

const GeneratedTypeInfo* GetGeneratedReflectionTypes() {
    return Types;
}

std::size_t GetGeneratedReflectionTypeCount() {
    return sizeof(Types) / sizeof(Types[0]);
}

} // namespace HE::Reflection::Generated
