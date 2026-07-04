#include "GeneratedReflection.h"

#include <string>
#include <string_view>

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

static void Serialize_HE__NameComponent(
    Serialization::SerializationBackend& backend,
    const std::string& name,
    const void* object) {
    const auto& component = *static_cast<const HE::NameComponent*>(object);
    backend.BeginObject(name);
    Serialization::SerializeValue(backend, "Name", component.Name);
    backend.EndObject();
}

static bool Deserialize_HE__NameComponent(
    Serialization::SerializationBackend& backend,
    const std::string& name,
    void* object) {
    auto& component = *static_cast<HE::NameComponent*>(object);
    bool success = true;
    backend.BeginObject(name);
    if (backend.HasField("Name")) {
        success &= Serialization::DeserializeValue(backend, "Name", component.Name);
    }
    backend.EndObject();
    return success;
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

static void Serialize_HE__Rendering__CameraComponent(
    Serialization::SerializationBackend& backend,
    const std::string& name,
    const void* object) {
    const auto& component = *static_cast<const HE::Rendering::CameraComponent*>(object);
    backend.BeginObject(name);
    Serialization::SerializeValue(backend, "Primary", component.Primary);
    Serialization::SerializeValue(backend, "FixedAspectRatio", component.FixedAspectRatio);
    backend.EndObject();
}

static bool Deserialize_HE__Rendering__CameraComponent(
    Serialization::SerializationBackend& backend,
    const std::string& name,
    void* object) {
    auto& component = *static_cast<HE::Rendering::CameraComponent*>(object);
    bool success = true;
    backend.BeginObject(name);
    if (backend.HasField("Primary")) {
        success &= Serialization::DeserializeValue(backend, "Primary", component.Primary);
    }
    if (backend.HasField("FixedAspectRatio")) {
        success &= Serialization::DeserializeValue(backend, "FixedAspectRatio", component.FixedAspectRatio);
    }
    backend.EndObject();
    return success;
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

static void Serialize_HE__Rendering__MaterialComponent(
    Serialization::SerializationBackend& backend,
    const std::string& name,
    const void* object) {
    const auto& component = *static_cast<const HE::Rendering::MaterialComponent*>(object);
    backend.BeginObject(name);
    Serialization::SerializeValue(backend, "MaterialInstance", component.MaterialInstance);
    backend.EndObject();
}

static bool Deserialize_HE__Rendering__MaterialComponent(
    Serialization::SerializationBackend& backend,
    const std::string& name,
    void* object) {
    auto& component = *static_cast<HE::Rendering::MaterialComponent*>(object);
    bool success = true;
    backend.BeginObject(name);
    if (backend.HasField("MaterialInstance")) {
        success &= Serialization::DeserializeValue(backend, "MaterialInstance", component.MaterialInstance);
    }
    backend.EndObject();
    return success;
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

static void Serialize_HE__Rendering__MeshComponent(
    Serialization::SerializationBackend& backend,
    const std::string& name,
    const void* object) {
    const auto& component = *static_cast<const HE::Rendering::MeshComponent*>(object);
    backend.BeginObject(name);
    Serialization::SerializeValue(backend, "MeshAssetName", component.MeshAssetName);
    backend.EndObject();
}

static bool Deserialize_HE__Rendering__MeshComponent(
    Serialization::SerializationBackend& backend,
    const std::string& name,
    void* object) {
    auto& component = *static_cast<HE::Rendering::MeshComponent*>(object);
    bool success = true;
    backend.BeginObject(name);
    if (backend.HasField("MeshAssetName")) {
        success &= Serialization::DeserializeValue(backend, "MeshAssetName", component.MeshAssetName);
    }
    backend.EndObject();
    return success;
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

static void Serialize_HE__TransformComponent(
    Serialization::SerializationBackend& backend,
    const std::string& name,
    const void* object) {
    const auto& component = *static_cast<const HE::TransformComponent*>(object);
    backend.BeginObject(name);
    Serialization::SerializeValue(backend, "Position", component.Position);
    Serialization::SerializeValue(backend, "Rotation", component.Rotation);
    Serialization::SerializeValue(backend, "Scale", component.Scale);
    backend.EndObject();
}

static bool Deserialize_HE__TransformComponent(
    Serialization::SerializationBackend& backend,
    const std::string& name,
    void* object) {
    auto& component = *static_cast<HE::TransformComponent*>(object);
    bool success = true;
    backend.BeginObject(name);
    if (backend.HasField("Position")) {
        success &= Serialization::DeserializeValue(backend, "Position", component.Position);
    }
    if (backend.HasField("Rotation")) {
        success &= Serialization::DeserializeValue(backend, "Rotation", component.Rotation);
    }
    if (backend.HasField("Scale")) {
        success &= Serialization::DeserializeValue(backend, "Scale", component.Scale);
    }
    backend.EndObject();
    return success;
}

static constexpr Refl::RuntimeFieldDescriptor RuntimeType0Fields[] = {
    {"Name", "std::string", "", ""},
};

static constexpr Refl::RuntimeFieldDescriptor RuntimeType1Fields[] = {
    {"Primary", "bool", "", ""},
    {"FixedAspectRatio", "bool", "", ""},
};

static constexpr Refl::RuntimeFieldDescriptor RuntimeType2Fields[] = {
    {"MaterialInstance", "Ref<HE::Rendering::MaterialInstance>", "", ""},
};

static constexpr Refl::RuntimeFieldDescriptor RuntimeType3Fields[] = {
    {"MeshAssetName", "std::string", "", ""},
};

static constexpr Refl::RuntimeFieldDescriptor RuntimeType4Fields[] = {
    {"Position", "glm::vec3", "", ""},
    {"Rotation", "glm::vec3", "", ""},
    {"Scale", "glm::vec3", "", ""},
};

static const Refl::RuntimeTypeDescriptor RuntimeTypes[] = {
    {"NameComponent", "HE::NameComponent", "component", "Name", "Core", ComponentTypeIdOf<HE::NameComponent>(), sizeof(HE::NameComponent), std::span<const Refl::RuntimeFieldDescriptor>{RuntimeType0Fields}, &ConstructDefault_HE__NameComponent, &Destroy_HE__NameComponent, &Copy_HE__NameComponent, &Serialize_HE__NameComponent, &Deserialize_HE__NameComponent, &AddCopyToWorld_HE__NameComponent},
    {"CameraComponent", "HE::Rendering::CameraComponent", "component", "Camera", "Rendering", ComponentTypeIdOf<HE::Rendering::CameraComponent>(), sizeof(HE::Rendering::CameraComponent), std::span<const Refl::RuntimeFieldDescriptor>{RuntimeType1Fields}, &ConstructDefault_HE__Rendering__CameraComponent, &Destroy_HE__Rendering__CameraComponent, &Copy_HE__Rendering__CameraComponent, &Serialize_HE__Rendering__CameraComponent, &Deserialize_HE__Rendering__CameraComponent, &AddCopyToWorld_HE__Rendering__CameraComponent},
    {"MaterialComponent", "HE::Rendering::MaterialComponent", "component", "Material", "Rendering", ComponentTypeIdOf<HE::Rendering::MaterialComponent>(), sizeof(HE::Rendering::MaterialComponent), std::span<const Refl::RuntimeFieldDescriptor>{RuntimeType2Fields}, &ConstructDefault_HE__Rendering__MaterialComponent, &Destroy_HE__Rendering__MaterialComponent, &Copy_HE__Rendering__MaterialComponent, &Serialize_HE__Rendering__MaterialComponent, &Deserialize_HE__Rendering__MaterialComponent, &AddCopyToWorld_HE__Rendering__MaterialComponent},
    {"MeshComponent", "HE::Rendering::MeshComponent", "component", "Mesh", "Rendering", ComponentTypeIdOf<HE::Rendering::MeshComponent>(), sizeof(HE::Rendering::MeshComponent), std::span<const Refl::RuntimeFieldDescriptor>{RuntimeType3Fields}, &ConstructDefault_HE__Rendering__MeshComponent, &Destroy_HE__Rendering__MeshComponent, &Copy_HE__Rendering__MeshComponent, &Serialize_HE__Rendering__MeshComponent, &Deserialize_HE__Rendering__MeshComponent, &AddCopyToWorld_HE__Rendering__MeshComponent},
    {"TransformComponent", "HE::TransformComponent", "component", "Transform", "Core", ComponentTypeIdOf<HE::TransformComponent>(), sizeof(HE::TransformComponent), std::span<const Refl::RuntimeFieldDescriptor>{RuntimeType4Fields}, &ConstructDefault_HE__TransformComponent, &Destroy_HE__TransformComponent, &Copy_HE__TransformComponent, &Serialize_HE__TransformComponent, &Deserialize_HE__TransformComponent, &AddCopyToWorld_HE__TransformComponent},
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

std::span<const RuntimeTypeDescriptor> GetRuntimeTypes() {
    return Generated::RuntimeTypes;
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

} // namespace HE::Refl
