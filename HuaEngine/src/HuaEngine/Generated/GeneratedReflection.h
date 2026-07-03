#include <cstddef>

#include "HuaEngine/Reflection/Reflection.h"

#ifndef HE_GENERATED_REFLECTION_METADATA_DECLARED
#define HE_GENERATED_REFLECTION_METADATA_DECLARED

namespace HE::Reflection::Generated {

struct GeneratedFieldInfo {
    const char* Name;
    const char* Type;
    const char* DisplayName;
    const char* Category;
};

struct GeneratedTypeInfo {
    const char* Name;
    const char* QualifiedName;
    const char* DisplayName;
    const char* Category;
    const char* Source;
    const GeneratedFieldInfo* Fields;
    std::size_t FieldCount;
};

const GeneratedTypeInfo* GetGeneratedReflectionTypes();
std::size_t GetGeneratedReflectionTypeCount();

} // namespace HE::Reflection::Generated

#endif // HE_GENERATED_REFLECTION_METADATA_DECLARED

#ifdef HE_GENERATED_REFLECTION_SOURCE_HUAENGINE_SRC_HUAENGINE_ECS_COMPONENTS_H
#ifndef HE_GENERATED_REFLECTION_TYPE_HE__NAMECOMPONENT
#define HE_GENERATED_REFLECTION_TYPE_HE__NAMECOMPONENT
srefl_class(HE::NameComponent,
    fields(
        field(Name)
    )
)
#endif // HE_GENERATED_REFLECTION_TYPE_HE__NAMECOMPONENT

#ifndef HE_GENERATED_REFLECTION_TYPE_HE__TRANSFORMCOMPONENT
#define HE_GENERATED_REFLECTION_TYPE_HE__TRANSFORMCOMPONENT
srefl_class(HE::TransformComponent,
    fields(
        field(Position),
        field(Rotation),
        field(Scale)
    )
)
#endif // HE_GENERATED_REFLECTION_TYPE_HE__TRANSFORMCOMPONENT

#endif // HE_GENERATED_REFLECTION_SOURCE_HUAENGINE_SRC_HUAENGINE_ECS_COMPONENTS_H

#ifdef HE_GENERATED_REFLECTION_SOURCE_HUAENGINE_SRC_MODULE_RENDERING_RENDERINGCOMPONENT_H
#ifndef HE_GENERATED_REFLECTION_TYPE_HE__RENDERING__CAMERACOMPONENT
#define HE_GENERATED_REFLECTION_TYPE_HE__RENDERING__CAMERACOMPONENT
srefl_class(HE::Rendering::CameraComponent,
    fields(
        field(Primary),
        field(FixedAspectRatio)
    )
)
#endif // HE_GENERATED_REFLECTION_TYPE_HE__RENDERING__CAMERACOMPONENT

#ifndef HE_GENERATED_REFLECTION_TYPE_HE__RENDERING__MATERIALCOMPONENT
#define HE_GENERATED_REFLECTION_TYPE_HE__RENDERING__MATERIALCOMPONENT
srefl_class(HE::Rendering::MaterialComponent,
    fields(
        field(MaterialInstance)
    )
)
#endif // HE_GENERATED_REFLECTION_TYPE_HE__RENDERING__MATERIALCOMPONENT

#ifndef HE_GENERATED_REFLECTION_TYPE_HE__RENDERING__MESHCOMPONENT
#define HE_GENERATED_REFLECTION_TYPE_HE__RENDERING__MESHCOMPONENT
srefl_class(HE::Rendering::MeshComponent,
    fields(
        field(MeshAssetName)
    )
)
#endif // HE_GENERATED_REFLECTION_TYPE_HE__RENDERING__MESHCOMPONENT

#endif // HE_GENERATED_REFLECTION_SOURCE_HUAENGINE_SRC_MODULE_RENDERING_RENDERINGCOMPONENT_H
