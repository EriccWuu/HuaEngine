#include "enginepch.h"
#include "SceneSerializer.h"

#include "HuaEngine/ECS/Runtime/OwnedValue.h"
#include "HuaEngine/ECS/Runtime/WorldScope.h"

namespace {
    void SerializeEntity(HE::Serialization::SerializationBackend& backend,
        const HE::Ecs::World& world, HE::EntityId entity) {
        backend.BeginObject("");
        backend.Serialize("uuid", HE::ToString(world.Uuid(entity)));
        backend.Serialize("name", std::string(world.Name(entity)));
        backend.BeginObject("components");
        for (const auto typeId : world.ListTypes(entity)) {
            const auto* type = world.Types().Find(typeId);
            if (!type || !type->Descriptor.Reflection) continue;
            if (type->Descriptor.Storage == HE::Ecs::StorageKind::Tag) {
                backend.BeginObject(type->Descriptor.Name);
                backend.EndObject();
                continue;
            }
            if (const void* component = world.TryGet(entity, typeId)) {
                HE::Refl::SerializeRuntimeObject(*type->Descriptor.Reflection, backend,
                    type->Descriptor.Name, component);
            }
        }
        backend.EndObject();
        backend.EndObject();
    }

    bool DeserializeEntity(HE::Serialization::SerializationBackend& backend, HE::Ecs::World& world) {
        backend.BeginObject("");
        std::string uuidText;
        backend.Deserialize("uuid", uuidText);
        std::string name = "Entity";
        backend.Deserialize("name", name);
        auto created = HE::Scene::CreateEntityInScope(world, name, HE::EntityUuid::FromString(uuidText));
        if (!created) { backend.EndObject(); return false; }
        bool success = true;
        if (backend.HasField("components")) {
            backend.BeginObject("components");
            for (const auto& componentName : backend.GetObjectKeys()) {
                const auto* type = world.Types().FindByName(componentName);
                // Unknown components retain the existing forward-compatible load behavior.
                if (!type) continue;
                if (!type->Descriptor.Reflection) { success = false; continue; }
                if (type->Descriptor.Storage == HE::Ecs::StorageKind::Tag) {
                    success &= static_cast<bool>(world.SetTag(created.Value(), type->Id));
                    continue;
                }
                auto value = HE::Ecs::OwnedValue::Default(*type);
                if (!value) { success = false; continue; }
                if (!HE::Refl::DeserializeRuntimeObject(*type->Descriptor.Reflection, backend,
                    componentName, value.Value().Data())) { success = false; continue; }
                success &= static_cast<bool>(world.Set(created.Value(), std::move(value).Value()));
            }
            backend.EndObject();
        }
        backend.EndObject();
        return success;
    }
}

namespace HE::Serialization {
    void Serializer<Scene>::Serialize(SerializationBackend& backend, const std::string& name, const Scene& scene) {
        auto scope = Ecs::WorldReadScope::Acquire(const_cast<Scene&>(scene).GetWorld());
        if (!scope) throw std::runtime_error(scope.GetError().Operation + ": " + scope.GetError().Message);
        const auto& world = scope.Value().Get();
        TypeRegistryScope types(backend, world.Types());
        if (!name.empty()) backend.BeginObject(name);
        backend.Serialize("name", scene.GetName().empty() ? "Untitled Scene" : scene.GetName());
        backend.Serialize("version", 3);
        backend.BeginArray("entities");
        uint32_t index = 0;
        for (const EntityId entity : world.Entities()) {
            backend.BeginArrayElement(index++);
            SerializeEntity(backend, world, entity);
            backend.EndArrayElement();
        }
        backend.EndArray();
        if (!name.empty()) backend.EndObject();
    }

    bool Serializer<Scene>::Deserialize(SerializationBackend& backend, const std::string& name, Scene& scene) {
        auto scope = Ecs::WorldEditScope::Acquire(scene.GetWorld());
        if (!scope) return false;
        auto& world = scope.Value().Get();
        TypeRegistryScope types(backend, world.Types());
        if (!world.Clear()) return false;
        if (!name.empty()) backend.BeginObject(name);
        std::string sceneName;
        if (backend.Deserialize("name", sceneName)) scene.SetName(sceneName);
        if (backend.HasField("version")) {
            uint32_t version = 0;
            backend.Deserialize("version", version);
        }
        if (!backend.HasField("entities")) {
            if (!name.empty()) backend.EndObject();
            return false;
        }
        bool success = true;
        const uint32_t count = static_cast<uint32_t>(backend.GetArraySize("entities"));
        backend.BeginArray("entities");
        for (uint32_t index = 0; index < count; ++index) {
            backend.BeginArrayElement(index);
            success &= DeserializeEntity(backend, world);
            backend.EndArrayElement();
        }
        backend.EndArray();
        if (!name.empty()) backend.EndObject();
        return success;
    }
}
