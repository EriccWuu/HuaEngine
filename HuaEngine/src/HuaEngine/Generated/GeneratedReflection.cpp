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

static void* ConstructDefault_HE__NameComponent() {
    return new HE::NameComponent();
}

static void Destroy_HE__NameComponent(void* object) {
    delete static_cast<HE::NameComponent*>(object);
}

static void* Copy_HE__NameComponent(const void* object) {
    return new HE::NameComponent(*static_cast<const HE::NameComponent*>(object));
}

static void AddCopyToWorld_HE__NameComponent(World& world, EntityId entity, const void* object) {
    world.AddComponent<HE::NameComponent>(entity, *static_cast<const HE::NameComponent*>(object));
}

static const void* GetConst_HE__NameComponent_Name(const void* object) {
    return &static_cast<const HE::NameComponent*>(object)->Name;
}

static void* GetMutable_HE__NameComponent_Name(void* object) {
    return &static_cast<HE::NameComponent*>(object)->Name;
}

static void Serialize_HE__NameComponent_Name(
    Serialization::SerializationBackend& backend,
    const std::string& name,
    const void* object) {
    const auto& component = *static_cast<const HE::NameComponent*>(object);
    Serialization::SerializeValue(backend, name, component.Name);
}

static bool Deserialize_HE__NameComponent_Name(
    Serialization::SerializationBackend& backend,
    const std::string& name,
    void* object) {
    auto& component = *static_cast<HE::NameComponent*>(object);
    auto fieldValue = component.Name;
    if (!Serialization::DeserializeValue(backend, name, fieldValue)) {
        return false;
    }
    component.Name = fieldValue;
    return true;
}

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

static const void* GetConst_HE__Rendering__MaterialComponent_MaterialInstance(const void* object) {
    return &static_cast<const HE::Rendering::MaterialComponent*>(object)->MaterialInstance;
}

static void* GetMutable_HE__Rendering__MaterialComponent_MaterialInstance(void* object) {
    return &static_cast<HE::Rendering::MaterialComponent*>(object)->MaterialInstance;
}

static void Serialize_HE__Rendering__MaterialComponent_MaterialInstance(
    Serialization::SerializationBackend& backend,
    const std::string& name,
    const void* object) {
    const auto& component = *static_cast<const HE::Rendering::MaterialComponent*>(object);
    Serialization::SerializeValue(backend, name, component.MaterialInstance);
}

static bool Deserialize_HE__Rendering__MaterialComponent_MaterialInstance(
    Serialization::SerializationBackend& backend,
    const std::string& name,
    void* object) {
    auto& component = *static_cast<HE::Rendering::MaterialComponent*>(object);
    auto fieldValue = component.MaterialInstance;
    if (!Serialization::DeserializeValue(backend, name, fieldValue)) {
        return false;
    }
    component.MaterialInstance = fieldValue;
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

static const void* GetConst_HE__Rendering__MeshComponent_MeshAssetName(const void* object) {
    return &static_cast<const HE::Rendering::MeshComponent*>(object)->MeshAssetName;
}

static void* GetMutable_HE__Rendering__MeshComponent_MeshAssetName(void* object) {
    return &static_cast<HE::Rendering::MeshComponent*>(object)->MeshAssetName;
}

static void Serialize_HE__Rendering__MeshComponent_MeshAssetName(
    Serialization::SerializationBackend& backend,
    const std::string& name,
    const void* object) {
    const auto& component = *static_cast<const HE::Rendering::MeshComponent*>(object);
    Serialization::SerializeValue(backend, name, component.MeshAssetName);
}

static bool Deserialize_HE__Rendering__MeshComponent_MeshAssetName(
    Serialization::SerializationBackend& backend,
    const std::string& name,
    void* object) {
    auto& component = *static_cast<HE::Rendering::MeshComponent*>(object);
    auto fieldValue = component.MeshAssetName;
    if (!Serialization::DeserializeValue(backend, name, fieldValue)) {
        return false;
    }
    component.MeshAssetName = fieldValue;
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
    {"Name", "std::string", "", "", offsetof(HE::NameComponent, Name), sizeof(static_cast<HE::NameComponent*>(nullptr)->Name), Refl::RuntimeFieldFlags::Serializable | Refl::RuntimeFieldFlags::ComponentField, &GetConst_HE__NameComponent_Name, &GetMutable_HE__NameComponent_Name, &Serialize_HE__NameComponent_Name, &Deserialize_HE__NameComponent_Name},
};

static constexpr Refl::RuntimeFieldDescriptor RuntimeType1Fields[] = {
    {"Primary", "bool", "", "", offsetof(HE::Rendering::CameraComponent, Primary), sizeof(static_cast<HE::Rendering::CameraComponent*>(nullptr)->Primary), Refl::RuntimeFieldFlags::Serializable | Refl::RuntimeFieldFlags::ComponentField, &GetConst_HE__Rendering__CameraComponent_Primary, &GetMutable_HE__Rendering__CameraComponent_Primary, &Serialize_HE__Rendering__CameraComponent_Primary, &Deserialize_HE__Rendering__CameraComponent_Primary},
    {"FixedAspectRatio", "bool", "", "", offsetof(HE::Rendering::CameraComponent, FixedAspectRatio), sizeof(static_cast<HE::Rendering::CameraComponent*>(nullptr)->FixedAspectRatio), Refl::RuntimeFieldFlags::Serializable | Refl::RuntimeFieldFlags::ComponentField, &GetConst_HE__Rendering__CameraComponent_FixedAspectRatio, &GetMutable_HE__Rendering__CameraComponent_FixedAspectRatio, &Serialize_HE__Rendering__CameraComponent_FixedAspectRatio, &Deserialize_HE__Rendering__CameraComponent_FixedAspectRatio},
};

static constexpr Refl::RuntimeFieldDescriptor RuntimeType2Fields[] = {
    {"MaterialInstance", "Ref<HE::Rendering::MaterialInstance>", "", "", offsetof(HE::Rendering::MaterialComponent, MaterialInstance), sizeof(static_cast<HE::Rendering::MaterialComponent*>(nullptr)->MaterialInstance), Refl::RuntimeFieldFlags::Serializable | Refl::RuntimeFieldFlags::ComponentField, &GetConst_HE__Rendering__MaterialComponent_MaterialInstance, &GetMutable_HE__Rendering__MaterialComponent_MaterialInstance, &Serialize_HE__Rendering__MaterialComponent_MaterialInstance, &Deserialize_HE__Rendering__MaterialComponent_MaterialInstance},
};

static constexpr Refl::RuntimeFieldDescriptor RuntimeType3Fields[] = {
    {"MeshAssetName", "std::string", "", "", offsetof(HE::Rendering::MeshComponent, MeshAssetName), sizeof(static_cast<HE::Rendering::MeshComponent*>(nullptr)->MeshAssetName), Refl::RuntimeFieldFlags::Serializable | Refl::RuntimeFieldFlags::ComponentField, &GetConst_HE__Rendering__MeshComponent_MeshAssetName, &GetMutable_HE__Rendering__MeshComponent_MeshAssetName, &Serialize_HE__Rendering__MeshComponent_MeshAssetName, &Deserialize_HE__Rendering__MeshComponent_MeshAssetName},
};

static constexpr Refl::RuntimeFieldDescriptor RuntimeType4Fields[] = {
    {"Position", "glm::vec3", "", "", offsetof(HE::TransformComponent, Position), sizeof(static_cast<HE::TransformComponent*>(nullptr)->Position), Refl::RuntimeFieldFlags::Serializable | Refl::RuntimeFieldFlags::ComponentField, &GetConst_HE__TransformComponent_Position, &GetMutable_HE__TransformComponent_Position, &Serialize_HE__TransformComponent_Position, &Deserialize_HE__TransformComponent_Position},
    {"Rotation", "glm::vec3", "", "", offsetof(HE::TransformComponent, Rotation), sizeof(static_cast<HE::TransformComponent*>(nullptr)->Rotation), Refl::RuntimeFieldFlags::Serializable | Refl::RuntimeFieldFlags::ComponentField, &GetConst_HE__TransformComponent_Rotation, &GetMutable_HE__TransformComponent_Rotation, &Serialize_HE__TransformComponent_Rotation, &Deserialize_HE__TransformComponent_Rotation},
    {"Scale", "glm::vec3", "", "", offsetof(HE::TransformComponent, Scale), sizeof(static_cast<HE::TransformComponent*>(nullptr)->Scale), Refl::RuntimeFieldFlags::Serializable | Refl::RuntimeFieldFlags::ComponentField, &GetConst_HE__TransformComponent_Scale, &GetMutable_HE__TransformComponent_Scale, &Serialize_HE__TransformComponent_Scale, &Deserialize_HE__TransformComponent_Scale},
};

static const Refl::RuntimeTypeDescriptor RuntimeTypes[] = {
    {"NameComponent", "HE::NameComponent", "component", "Name", "Core", ComponentTypeIdOf<HE::NameComponent>(), sizeof(HE::NameComponent), std::span<const Refl::RuntimeFieldDescriptor>{RuntimeType0Fields}, &ConstructDefault_HE__NameComponent, &Destroy_HE__NameComponent, &Copy_HE__NameComponent, nullptr, nullptr, &AddCopyToWorld_HE__NameComponent},
    {"CameraComponent", "HE::Rendering::CameraComponent", "component", "Camera", "Rendering", ComponentTypeIdOf<HE::Rendering::CameraComponent>(), sizeof(HE::Rendering::CameraComponent), std::span<const Refl::RuntimeFieldDescriptor>{RuntimeType1Fields}, &ConstructDefault_HE__Rendering__CameraComponent, &Destroy_HE__Rendering__CameraComponent, &Copy_HE__Rendering__CameraComponent, nullptr, nullptr, &AddCopyToWorld_HE__Rendering__CameraComponent},
    {"MaterialComponent", "HE::Rendering::MaterialComponent", "component", "Material", "Rendering", ComponentTypeIdOf<HE::Rendering::MaterialComponent>(), sizeof(HE::Rendering::MaterialComponent), std::span<const Refl::RuntimeFieldDescriptor>{RuntimeType2Fields}, &ConstructDefault_HE__Rendering__MaterialComponent, &Destroy_HE__Rendering__MaterialComponent, &Copy_HE__Rendering__MaterialComponent, nullptr, nullptr, &AddCopyToWorld_HE__Rendering__MaterialComponent},
    {"MeshComponent", "HE::Rendering::MeshComponent", "component", "Mesh", "Rendering", ComponentTypeIdOf<HE::Rendering::MeshComponent>(), sizeof(HE::Rendering::MeshComponent), std::span<const Refl::RuntimeFieldDescriptor>{RuntimeType3Fields}, &ConstructDefault_HE__Rendering__MeshComponent, &Destroy_HE__Rendering__MeshComponent, &Copy_HE__Rendering__MeshComponent, nullptr, nullptr, &AddCopyToWorld_HE__Rendering__MeshComponent},
    {"TransformComponent", "HE::TransformComponent", "component", "Transform", "Core", ComponentTypeIdOf<HE::TransformComponent>(), sizeof(HE::TransformComponent), std::span<const Refl::RuntimeFieldDescriptor>{RuntimeType4Fields}, &ConstructDefault_HE__TransformComponent, &Destroy_HE__TransformComponent, &Copy_HE__TransformComponent, nullptr, nullptr, &AddCopyToWorld_HE__TransformComponent},
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
    if (!name.empty() && !backend.HasField(name)) {
        return false;
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
