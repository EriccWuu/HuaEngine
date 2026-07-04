#include "GeneratedReflection.h"

#include <string>
#include <string_view>
#include <vector>

#include "HuaEngine/ECS/ComponentRegistry.h"
#include "HuaEngine/ECS/ComponentType.h"
#include "HuaEngine/ECS/World.h"
#include "HuaEngine/Reflection/Reflection.h"
#include "HuaEngine/Serialization/Serialization.h"
#include "HuaEngine/ECS/Components.h"
#include "Module/Rendering/RenderingComponent.h"

namespace HE::Generated {

static constexpr Refl::RuntimeEnumValueDescriptor RuntimeEnum0Values[] = {
    {"Opaque", 0, ""},
    {"Masked", 1, ""},
    {"Transparent", 2, ""},
};

static constexpr ReflectedEnumValueInfo ReflectedEnum0Values[] = {
    {"Opaque", 0, ""},
    {"Masked", 1, ""},
    {"Transparent", 2, ""},
};

static constexpr Refl::RuntimeEnumDescriptor RuntimeEnums[] = {
    {"MaterialBlendMode", "HE::Rendering::MaterialBlendMode", "int", std::span<const Refl::RuntimeEnumValueDescriptor>{RuntimeEnum0Values}},
};

static constexpr ReflectedEnumInfo ReflectedEnums[] = {
    {"MaterialBlendMode", "HE::Rendering::MaterialBlendMode", "int", std::span<const ReflectedEnumValueInfo>{ReflectedEnum0Values}},
};

static constexpr ReflectedFieldInfo Type0Fields[] = {
    {"Primary", "bool"},
    {"FixedAspectRatio", "bool"},
};

static constexpr ReflectedFieldInfo Type1Fields[] = {
    {"Material", "MaterialAssetRef"},
    {"Overrides", "MaterialOverrideSet"},
    {"BlendMode", "MaterialBlendMode"},
};

static constexpr ReflectedFieldInfo Type2Fields[] = {
    {"Mesh", "MeshAssetRef"},
};

static constexpr ReflectedFieldInfo Type3Fields[] = {
    {"Position", "glm::vec3"},
    {"Rotation", "glm::vec3"},
    {"Scale", "glm::vec3"},
};

static void* ConstructDefault_HE__Rendering__CameraComponent() {
    return new HE::Rendering::CameraComponent();
}

static void Destroy_HE__Rendering__CameraComponent(void* object) {
    delete static_cast<HE::Rendering::CameraComponent*>(object);
}

static void* Copy_HE__Rendering__CameraComponent(const void* object) {
    return new HE::Rendering::CameraComponent(*static_cast<const HE::Rendering::CameraComponent*>(object));
}

static void AddCopyToWorld_HE__Rendering__CameraComponent(World& world, EntityId entity, const void* object) {
    world.AddComponent<HE::Rendering::CameraComponent>(entity, *static_cast<const HE::Rendering::CameraComponent*>(object));
}

static const void* GetConst_HE__Rendering__CameraComponent_Primary(const void* object) {
    return &static_cast<const HE::Rendering::CameraComponent*>(object)->Primary;
}

static void* GetMutable_HE__Rendering__CameraComponent_Primary(void* object) {
    return &static_cast<HE::Rendering::CameraComponent*>(object)->Primary;
}

static void Serialize_HE__Rendering__CameraComponent_Primary(
    Serialization::SerializationBackend& backend,
    const std::string& name,
    const void* object) {
    const auto& component = *static_cast<const HE::Rendering::CameraComponent*>(object);
    Serialization::SerializeValue(backend, name, component.Primary);
}

static bool Deserialize_HE__Rendering__CameraComponent_Primary(
    Serialization::SerializationBackend& backend,
    const std::string& name,
    void* object) {
    auto& component = *static_cast<HE::Rendering::CameraComponent*>(object);
    auto fieldValue = component.Primary;
    if (!Serialization::DeserializeValue(backend, name, fieldValue)) {
        return false;
    }
    component.Primary = fieldValue;
    return true;
}

static const void* GetConst_HE__Rendering__CameraComponent_FixedAspectRatio(const void* object) {
    return &static_cast<const HE::Rendering::CameraComponent*>(object)->FixedAspectRatio;
}

static void* GetMutable_HE__Rendering__CameraComponent_FixedAspectRatio(void* object) {
    return &static_cast<HE::Rendering::CameraComponent*>(object)->FixedAspectRatio;
}

static void Serialize_HE__Rendering__CameraComponent_FixedAspectRatio(
    Serialization::SerializationBackend& backend,
    const std::string& name,
    const void* object) {
    const auto& component = *static_cast<const HE::Rendering::CameraComponent*>(object);
    Serialization::SerializeValue(backend, name, component.FixedAspectRatio);
}

static bool Deserialize_HE__Rendering__CameraComponent_FixedAspectRatio(
    Serialization::SerializationBackend& backend,
    const std::string& name,
    void* object) {
    auto& component = *static_cast<HE::Rendering::CameraComponent*>(object);
    auto fieldValue = component.FixedAspectRatio;
    if (!Serialization::DeserializeValue(backend, name, fieldValue)) {
        return false;
    }
    component.FixedAspectRatio = fieldValue;
    return true;
}

static void* ConstructDefault_HE__Rendering__MaterialComponent() {
    return new HE::Rendering::MaterialComponent();
}

static void Destroy_HE__Rendering__MaterialComponent(void* object) {
    delete static_cast<HE::Rendering::MaterialComponent*>(object);
}

static void* Copy_HE__Rendering__MaterialComponent(const void* object) {
    return new HE::Rendering::MaterialComponent(*static_cast<const HE::Rendering::MaterialComponent*>(object));
}

static void AddCopyToWorld_HE__Rendering__MaterialComponent(World& world, EntityId entity, const void* object) {
    world.AddComponent<HE::Rendering::MaterialComponent>(entity, *static_cast<const HE::Rendering::MaterialComponent*>(object));
}

static const void* GetConst_HE__Rendering__MaterialComponent_Material(const void* object) {
    return &static_cast<const HE::Rendering::MaterialComponent*>(object)->Material;
}

static void* GetMutable_HE__Rendering__MaterialComponent_Material(void* object) {
    return &static_cast<HE::Rendering::MaterialComponent*>(object)->Material;
}

static void Serialize_HE__Rendering__MaterialComponent_Material(
    Serialization::SerializationBackend& backend,
    const std::string& name,
    const void* object) {
    const auto& component = *static_cast<const HE::Rendering::MaterialComponent*>(object);
    Serialization::SerializeValue(backend, name, component.Material);
}

static bool Deserialize_HE__Rendering__MaterialComponent_Material(
    Serialization::SerializationBackend& backend,
    const std::string& name,
    void* object) {
    auto& component = *static_cast<HE::Rendering::MaterialComponent*>(object);
    auto fieldValue = component.Material;
    if (!Serialization::DeserializeValue(backend, name, fieldValue)) {
        return false;
    }
    component.Material = fieldValue;
    return true;
}

static const void* GetConst_HE__Rendering__MaterialComponent_Overrides(const void* object) {
    return &static_cast<const HE::Rendering::MaterialComponent*>(object)->Overrides;
}

static void* GetMutable_HE__Rendering__MaterialComponent_Overrides(void* object) {
    return &static_cast<HE::Rendering::MaterialComponent*>(object)->Overrides;
}

static void Serialize_HE__Rendering__MaterialComponent_Overrides(
    Serialization::SerializationBackend& backend,
    const std::string& name,
    const void* object) {
    const auto& component = *static_cast<const HE::Rendering::MaterialComponent*>(object);
    Serialization::SerializeValue(backend, name, component.Overrides);
}

static bool Deserialize_HE__Rendering__MaterialComponent_Overrides(
    Serialization::SerializationBackend& backend,
    const std::string& name,
    void* object) {
    auto& component = *static_cast<HE::Rendering::MaterialComponent*>(object);
    auto fieldValue = component.Overrides;
    if (!Serialization::DeserializeValue(backend, name, fieldValue)) {
        return false;
    }
    component.Overrides = fieldValue;
    return true;
}

static const void* GetConst_HE__Rendering__MaterialComponent_BlendMode(const void* object) {
    return &static_cast<const HE::Rendering::MaterialComponent*>(object)->BlendMode;
}

static void* GetMutable_HE__Rendering__MaterialComponent_BlendMode(void* object) {
    return &static_cast<HE::Rendering::MaterialComponent*>(object)->BlendMode;
}

static void Serialize_HE__Rendering__MaterialComponent_BlendMode(
    Serialization::SerializationBackend& backend,
    const std::string& name,
    const void* object) {
    const auto& component = *static_cast<const HE::Rendering::MaterialComponent*>(object);
    const auto enumValue = static_cast<int64_t>(component.BlendMode);
    if (const auto* value = Refl::FindRuntimeEnumValueByValue(*&RuntimeEnums[0], enumValue)) {
        backend.Serialize(name, std::string(value->Name));
    }
}

static bool Deserialize_HE__Rendering__MaterialComponent_BlendMode(
    Serialization::SerializationBackend& backend,
    const std::string& name,
    void* object) {
    auto& component = *static_cast<HE::Rendering::MaterialComponent*>(object);
    std::string enumName;
    if (!backend.Deserialize(name, enumName)) {
        return false;
    }
    const auto* value = Refl::FindRuntimeEnumValueByName(*&RuntimeEnums[0], enumName);
    if (value == nullptr) {
        return false;
    }
    component.BlendMode = static_cast<HE::Rendering::MaterialBlendMode>(value->Value);
    return true;
}

static void* ConstructDefault_HE__Rendering__MeshComponent() {
    return new HE::Rendering::MeshComponent();
}

static void Destroy_HE__Rendering__MeshComponent(void* object) {
    delete static_cast<HE::Rendering::MeshComponent*>(object);
}

static void* Copy_HE__Rendering__MeshComponent(const void* object) {
    return new HE::Rendering::MeshComponent(*static_cast<const HE::Rendering::MeshComponent*>(object));
}

static void AddCopyToWorld_HE__Rendering__MeshComponent(World& world, EntityId entity, const void* object) {
    world.AddComponent<HE::Rendering::MeshComponent>(entity, *static_cast<const HE::Rendering::MeshComponent*>(object));
}

static const void* GetConst_HE__Rendering__MeshComponent_Mesh(const void* object) {
    return &static_cast<const HE::Rendering::MeshComponent*>(object)->Mesh;
}

static void* GetMutable_HE__Rendering__MeshComponent_Mesh(void* object) {
    return &static_cast<HE::Rendering::MeshComponent*>(object)->Mesh;
}

static void Serialize_HE__Rendering__MeshComponent_Mesh(
    Serialization::SerializationBackend& backend,
    const std::string& name,
    const void* object) {
    const auto& component = *static_cast<const HE::Rendering::MeshComponent*>(object);
    Serialization::SerializeValue(backend, name, component.Mesh);
}

static bool Deserialize_HE__Rendering__MeshComponent_Mesh(
    Serialization::SerializationBackend& backend,
    const std::string& name,
    void* object) {
    auto& component = *static_cast<HE::Rendering::MeshComponent*>(object);
    auto fieldValue = component.Mesh;
    if (!Serialization::DeserializeValue(backend, name, fieldValue)) {
        return false;
    }
    component.Mesh = fieldValue;
    return true;
}

static void* ConstructDefault_HE__TransformComponent() {
    return new HE::TransformComponent();
}

static void Destroy_HE__TransformComponent(void* object) {
    delete static_cast<HE::TransformComponent*>(object);
}

static void* Copy_HE__TransformComponent(const void* object) {
    return new HE::TransformComponent(*static_cast<const HE::TransformComponent*>(object));
}

static void AddCopyToWorld_HE__TransformComponent(World& world, EntityId entity, const void* object) {
    world.AddComponent<HE::TransformComponent>(entity, *static_cast<const HE::TransformComponent*>(object));
}

static const void* GetConst_HE__TransformComponent_Position(const void* object) {
    return &static_cast<const HE::TransformComponent*>(object)->Position;
}

static void* GetMutable_HE__TransformComponent_Position(void* object) {
    return &static_cast<HE::TransformComponent*>(object)->Position;
}

static void Serialize_HE__TransformComponent_Position(
    Serialization::SerializationBackend& backend,
    const std::string& name,
    const void* object) {
    const auto& component = *static_cast<const HE::TransformComponent*>(object);
    Serialization::SerializeValue(backend, name, component.Position);
}

static bool Deserialize_HE__TransformComponent_Position(
    Serialization::SerializationBackend& backend,
    const std::string& name,
    void* object) {
    auto& component = *static_cast<HE::TransformComponent*>(object);
    auto fieldValue = component.Position;
    if (!Serialization::DeserializeValue(backend, name, fieldValue)) {
        return false;
    }
    component.Position = fieldValue;
    return true;
}

static const void* GetConst_HE__TransformComponent_Rotation(const void* object) {
    return &static_cast<const HE::TransformComponent*>(object)->Rotation;
}

static void* GetMutable_HE__TransformComponent_Rotation(void* object) {
    return &static_cast<HE::TransformComponent*>(object)->Rotation;
}

static void Serialize_HE__TransformComponent_Rotation(
    Serialization::SerializationBackend& backend,
    const std::string& name,
    const void* object) {
    const auto& component = *static_cast<const HE::TransformComponent*>(object);
    Serialization::SerializeValue(backend, name, component.Rotation);
}

static bool Deserialize_HE__TransformComponent_Rotation(
    Serialization::SerializationBackend& backend,
    const std::string& name,
    void* object) {
    auto& component = *static_cast<HE::TransformComponent*>(object);
    auto fieldValue = component.Rotation;
    if (!Serialization::DeserializeValue(backend, name, fieldValue)) {
        return false;
    }
    component.Rotation = fieldValue;
    return true;
}

static const void* GetConst_HE__TransformComponent_Scale(const void* object) {
    return &static_cast<const HE::TransformComponent*>(object)->Scale;
}

static void* GetMutable_HE__TransformComponent_Scale(void* object) {
    return &static_cast<HE::TransformComponent*>(object)->Scale;
}

static void Serialize_HE__TransformComponent_Scale(
    Serialization::SerializationBackend& backend,
    const std::string& name,
    const void* object) {
    const auto& component = *static_cast<const HE::TransformComponent*>(object);
    Serialization::SerializeValue(backend, name, component.Scale);
}

static bool Deserialize_HE__TransformComponent_Scale(
    Serialization::SerializationBackend& backend,
    const std::string& name,
    void* object) {
    auto& component = *static_cast<HE::TransformComponent*>(object);
    auto fieldValue = component.Scale;
    if (!Serialization::DeserializeValue(backend, name, fieldValue)) {
        return false;
    }
    component.Scale = fieldValue;
    return true;
}

static constexpr Refl::RuntimeFieldDescriptor RuntimeType0Fields[] = {
    {"Primary", "bool", "", "", offsetof(HE::Rendering::CameraComponent, Primary), sizeof(static_cast<HE::Rendering::CameraComponent*>(nullptr)->Primary), Refl::RuntimeFieldFlags::Serializable | Refl::RuntimeFieldFlags::ComponentField | Refl::RuntimeFieldFlags::Editable, &GetConst_HE__Rendering__CameraComponent_Primary, &GetMutable_HE__Rendering__CameraComponent_Primary, &Serialize_HE__Rendering__CameraComponent_Primary, &Deserialize_HE__Rendering__CameraComponent_Primary, nullptr},
    {"FixedAspectRatio", "bool", "", "", offsetof(HE::Rendering::CameraComponent, FixedAspectRatio), sizeof(static_cast<HE::Rendering::CameraComponent*>(nullptr)->FixedAspectRatio), Refl::RuntimeFieldFlags::Serializable | Refl::RuntimeFieldFlags::ComponentField | Refl::RuntimeFieldFlags::Editable, &GetConst_HE__Rendering__CameraComponent_FixedAspectRatio, &GetMutable_HE__Rendering__CameraComponent_FixedAspectRatio, &Serialize_HE__Rendering__CameraComponent_FixedAspectRatio, &Deserialize_HE__Rendering__CameraComponent_FixedAspectRatio, nullptr},
};

static constexpr Refl::RuntimeFieldDescriptor RuntimeType1Fields[] = {
    {"Material", "MaterialAssetRef", "", "", offsetof(HE::Rendering::MaterialComponent, Material), sizeof(static_cast<HE::Rendering::MaterialComponent*>(nullptr)->Material), Refl::RuntimeFieldFlags::Serializable | Refl::RuntimeFieldFlags::ComponentField, &GetConst_HE__Rendering__MaterialComponent_Material, &GetMutable_HE__Rendering__MaterialComponent_Material, &Serialize_HE__Rendering__MaterialComponent_Material, &Deserialize_HE__Rendering__MaterialComponent_Material, nullptr},
    {"Overrides", "MaterialOverrideSet", "", "", offsetof(HE::Rendering::MaterialComponent, Overrides), sizeof(static_cast<HE::Rendering::MaterialComponent*>(nullptr)->Overrides), Refl::RuntimeFieldFlags::Serializable | Refl::RuntimeFieldFlags::ComponentField, &GetConst_HE__Rendering__MaterialComponent_Overrides, &GetMutable_HE__Rendering__MaterialComponent_Overrides, &Serialize_HE__Rendering__MaterialComponent_Overrides, &Deserialize_HE__Rendering__MaterialComponent_Overrides, nullptr},
    {"BlendMode", "MaterialBlendMode", "", "", offsetof(HE::Rendering::MaterialComponent, BlendMode), sizeof(static_cast<HE::Rendering::MaterialComponent*>(nullptr)->BlendMode), Refl::RuntimeFieldFlags::Serializable | Refl::RuntimeFieldFlags::ComponentField | Refl::RuntimeFieldFlags::Editable, &GetConst_HE__Rendering__MaterialComponent_BlendMode, &GetMutable_HE__Rendering__MaterialComponent_BlendMode, &Serialize_HE__Rendering__MaterialComponent_BlendMode, &Deserialize_HE__Rendering__MaterialComponent_BlendMode, &RuntimeEnums[0]},
};

static constexpr Refl::RuntimeFieldDescriptor RuntimeType2Fields[] = {
    {"Mesh", "MeshAssetRef", "", "", offsetof(HE::Rendering::MeshComponent, Mesh), sizeof(static_cast<HE::Rendering::MeshComponent*>(nullptr)->Mesh), Refl::RuntimeFieldFlags::Serializable | Refl::RuntimeFieldFlags::ComponentField, &GetConst_HE__Rendering__MeshComponent_Mesh, &GetMutable_HE__Rendering__MeshComponent_Mesh, &Serialize_HE__Rendering__MeshComponent_Mesh, &Deserialize_HE__Rendering__MeshComponent_Mesh, nullptr},
};

static constexpr Refl::RuntimeFieldDescriptor RuntimeType3Fields[] = {
    {"Position", "glm::vec3", "", "", offsetof(HE::TransformComponent, Position), sizeof(static_cast<HE::TransformComponent*>(nullptr)->Position), Refl::RuntimeFieldFlags::Serializable | Refl::RuntimeFieldFlags::ComponentField | Refl::RuntimeFieldFlags::Editable, &GetConst_HE__TransformComponent_Position, &GetMutable_HE__TransformComponent_Position, &Serialize_HE__TransformComponent_Position, &Deserialize_HE__TransformComponent_Position, nullptr},
    {"Rotation", "glm::vec3", "", "", offsetof(HE::TransformComponent, Rotation), sizeof(static_cast<HE::TransformComponent*>(nullptr)->Rotation), Refl::RuntimeFieldFlags::Serializable | Refl::RuntimeFieldFlags::ComponentField | Refl::RuntimeFieldFlags::Editable, &GetConst_HE__TransformComponent_Rotation, &GetMutable_HE__TransformComponent_Rotation, &Serialize_HE__TransformComponent_Rotation, &Deserialize_HE__TransformComponent_Rotation, nullptr},
    {"Scale", "glm::vec3", "", "", offsetof(HE::TransformComponent, Scale), sizeof(static_cast<HE::TransformComponent*>(nullptr)->Scale), Refl::RuntimeFieldFlags::Serializable | Refl::RuntimeFieldFlags::ComponentField | Refl::RuntimeFieldFlags::Editable, &GetConst_HE__TransformComponent_Scale, &GetMutable_HE__TransformComponent_Scale, &Serialize_HE__TransformComponent_Scale, &Deserialize_HE__TransformComponent_Scale, nullptr},
};

static const Refl::RuntimeTypeDescriptor RuntimeTypes[] = {
    {"CameraComponent", "HE::Rendering::CameraComponent", "component", "Camera", "Rendering", ComponentTypeIdOf<HE::Rendering::CameraComponent>(), sizeof(HE::Rendering::CameraComponent), std::span<const Refl::RuntimeFieldDescriptor>{RuntimeType0Fields}, &ConstructDefault_HE__Rendering__CameraComponent, &Destroy_HE__Rendering__CameraComponent, &Copy_HE__Rendering__CameraComponent, nullptr, nullptr, &AddCopyToWorld_HE__Rendering__CameraComponent},
    {"MaterialComponent", "HE::Rendering::MaterialComponent", "component", "Material", "Rendering", ComponentTypeIdOf<HE::Rendering::MaterialComponent>(), sizeof(HE::Rendering::MaterialComponent), std::span<const Refl::RuntimeFieldDescriptor>{RuntimeType1Fields}, &ConstructDefault_HE__Rendering__MaterialComponent, &Destroy_HE__Rendering__MaterialComponent, &Copy_HE__Rendering__MaterialComponent, nullptr, nullptr, &AddCopyToWorld_HE__Rendering__MaterialComponent},
    {"MeshComponent", "HE::Rendering::MeshComponent", "component", "Mesh", "Rendering", ComponentTypeIdOf<HE::Rendering::MeshComponent>(), sizeof(HE::Rendering::MeshComponent), std::span<const Refl::RuntimeFieldDescriptor>{RuntimeType2Fields}, &ConstructDefault_HE__Rendering__MeshComponent, &Destroy_HE__Rendering__MeshComponent, &Copy_HE__Rendering__MeshComponent, nullptr, nullptr, &AddCopyToWorld_HE__Rendering__MeshComponent},
    {"TransformComponent", "HE::TransformComponent", "component", "Transform", "Core", ComponentTypeIdOf<HE::TransformComponent>(), sizeof(HE::TransformComponent), std::span<const Refl::RuntimeFieldDescriptor>{RuntimeType3Fields}, &ConstructDefault_HE__TransformComponent, &Destroy_HE__TransformComponent, &Copy_HE__TransformComponent, nullptr, nullptr, &AddCopyToWorld_HE__TransformComponent},
};

static constexpr ReflectedTypeInfo Types[] = {
    {"CameraComponent", "HE::Rendering::CameraComponent", "component", "Camera", "Rendering", std::span<const ReflectedFieldInfo>{Type0Fields}},
    {"MaterialComponent", "HE::Rendering::MaterialComponent", "component", "Material", "Rendering", std::span<const ReflectedFieldInfo>{Type1Fields}},
    {"MeshComponent", "HE::Rendering::MeshComponent", "component", "Mesh", "Rendering", std::span<const ReflectedFieldInfo>{Type2Fields}},
    {"TransformComponent", "HE::TransformComponent", "component", "Transform", "Core", std::span<const ReflectedFieldInfo>{Type3Fields}},
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

std::span<const ReflectedEnumInfo> GetReflectedEnums() {
    return ReflectedEnums;
}

const ReflectedEnumInfo* FindReflectedEnum(std::string_view qualifiedName) {
    for (const ReflectedEnumInfo& enumType : GetReflectedEnums()) {
        if (enumType.QualifiedName == qualifiedName) {
            return &enumType;
        }
    }

    return nullptr;
}

void RegisterGeneratedComponents(ComponentRegistry& registry) {
    for (const Refl::RuntimeTypeDescriptor& type : Refl::GetRuntimeTypes()) {
        if (type.Kind == "component") {
            registry.Register(type);
        }
    }
}

} // namespace HE::Generated

namespace HE::Refl {

namespace {
using RuntimeTypeProvider = std::span<const RuntimeTypeDescriptor> (*)();

std::span<const RuntimeTypeDescriptor> GetGeneratedRuntimeTypes() {
    return Generated::RuntimeTypes;
}

std::span<const RuntimeTypeProvider> GetRuntimeTypeProviders() {
    static const RuntimeTypeProvider providers[] = {
        &GetGeneratedRuntimeTypes,
    };
    return providers;
}

const std::vector<RuntimeTypeDescriptor>& GetRuntimeTypeCache() {
    static const std::vector<RuntimeTypeDescriptor> types = [] {
        std::vector<RuntimeTypeDescriptor> result;
        for (RuntimeTypeProvider provider : GetRuntimeTypeProviders()) {
            const std::span<const RuntimeTypeDescriptor> providedTypes = provider();
            result.insert(result.end(), providedTypes.begin(), providedTypes.end());
        }
        return result;
    }();
    return types;
}

} // namespace

std::span<const RuntimeTypeDescriptor> GetRuntimeTypes() {
    const std::vector<RuntimeTypeDescriptor>& types = GetRuntimeTypeCache();
    return std::span<const RuntimeTypeDescriptor>{ types.data(), types.size() };
}

const RuntimeTypeDescriptor* FindRuntimeType(std::string_view qualifiedName) {
    for (const RuntimeTypeDescriptor& type : GetRuntimeTypes()) {
        if (type.QualifiedName == qualifiedName) {
            return &type;
        }
    }

    return nullptr;
}

const RuntimeTypeDescriptor* FindRuntimeType(ComponentTypeId typeId) {
    for (const RuntimeTypeDescriptor& type : GetRuntimeTypes()) {
        if (type.TypeId == typeId) {
            return &type;
        }
    }

    return nullptr;
}

std::span<const RuntimeEnumDescriptor> GetRuntimeEnums() {
    return Generated::RuntimeEnums;
}

const RuntimeEnumDescriptor* FindRuntimeEnum(std::string_view qualifiedName) {
    for (const RuntimeEnumDescriptor& enumType : GetRuntimeEnums()) {
        if (enumType.QualifiedName == qualifiedName) {
            return &enumType;
        }
    }

    return nullptr;
}

const RuntimeEnumValueDescriptor* FindRuntimeEnumValueByName(
    const RuntimeEnumDescriptor& enumType,
    std::string_view name) {
    for (const RuntimeEnumValueDescriptor& value : enumType.Values) {
        if (value.Name == name) {
            return &value;
        }
    }
    return nullptr;
}

const RuntimeEnumValueDescriptor* FindRuntimeEnumValueByValue(
    const RuntimeEnumDescriptor& enumType,
    int64_t value) {
    for (const RuntimeEnumValueDescriptor& enumValue : enumType.Values) {
        if (enumValue.Value == value) {
            return &enumValue;
        }
    }
    return nullptr;
}

void SerializeRuntimeObject(
    const RuntimeTypeDescriptor& type,
    Serialization::SerializationBackend& backend,
    const std::string& name,
    const void* object) {
    backend.BeginObject(name);
    for (const RuntimeFieldDescriptor& field : type.Fields) {
        if (!HasRuntimeFieldFlag(field.Flags, RuntimeFieldFlags::Serializable) || field.Serialize == nullptr) {
            continue;
        }
        field.Serialize(backend, std::string(field.Name), object);
    }
    backend.EndObject();
}

bool DeserializeRuntimeObject(
    const RuntimeTypeDescriptor& type,
    Serialization::SerializationBackend& backend,
    const std::string& name,
    void* object) {
    if (!name.empty()) {
        if (!backend.HasField(name) || backend.GetFieldType(name) != Serialization::SerializationType::Object) {
            return false;
        }
    }

    backend.BeginObject(name);
    bool success = true;
    for (const RuntimeFieldDescriptor& field : type.Fields) {
        if (!HasRuntimeFieldFlag(field.Flags, RuntimeFieldFlags::Serializable) || field.Deserialize == nullptr) {
            continue;
        }
        const std::string fieldName(field.Name);
        if (!backend.HasField(fieldName)) {
            continue;
        }
        if (!field.Deserialize(backend, fieldName, object)) {
            success = false;
        }
    }
    backend.EndObject();
    return success;
}

} // namespace HE::Refl
