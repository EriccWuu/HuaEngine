#include "enginepch.h"
#include "SceneSerializer.h"
#include "Module/Rendering/RenderingComponent.h"
#include <functional>
#include <unordered_map>
#include <type_traits>
#include "HuaEngine/Math/Math.h"

namespace HE {

    // Component serialization registry with name mapping
    struct ComponentSerializers {
        using SerializeFunc = std::function<void(HE::Serialization::SerializationBackend&, entt::registry&, entt::entity, const std::string&)>;
        using DeserializeFunc = std::function<void(HE::Serialization::SerializationBackend&, entt::registry&, entt::entity, const std::string&)>;

        std::unordered_map<entt::id_type, SerializeFunc> serializeFuncs;
        std::unordered_map<entt::id_type, DeserializeFunc> deserializeFuncs;

        // Bidirectional name mapping
        std::unordered_map<entt::id_type, std::string> typeIdToName;
        std::unordered_map<std::string, entt::id_type> nameToTypeId;

        template<typename T>
        void RegisterComponent(const std::string& componentName) {
            constexpr auto typeId = entt::type_hash<T>::value();

            // Register name mapping
            typeIdToName[typeId] = componentName;
            nameToTypeId[componentName] = typeId;

            // Register serialization function - now takes componentName as parameter
            serializeFuncs[typeId] = [](HE::Serialization::SerializationBackend& backend, entt::registry& registry, entt::entity entity, const std::string& name) {
                if (registry.all_of<T>(entity)) {
                    auto& component = registry.get<T>(entity);
                    HE::Serialization::Serializer<T>::Serialize(backend, name, component);
                }
            };

            // Register deserialization function - now takes componentName as parameter
            deserializeFuncs[typeId] = [typeId](HE::Serialization::SerializationBackend& backend, entt::registry& registry, entt::entity entity, const std::string& name) {
                T component{};
                if (HE::Serialization::Serializer<T>::Deserialize(backend, name, component)) {
                    registry.emplace_or_replace<T>(entity, std::move(component));
                }
                else {
                    HE_CORE_WARN("Failed to deserialize component {0}", typeId);
                }
            };
        }

        std::string GetComponentName(entt::id_type typeId) const {
            auto it = typeIdToName.find(typeId);
            return it != typeIdToName.end() ? it->second : "";
        }

        entt::id_type GetTypeId(const std::string& name) const {
            auto it = nameToTypeId.find(name);
            return it != nameToTypeId.end() ? it->second : 0;
        }

        static ComponentSerializers& Instance() {
            static ComponentSerializers instance;
            static bool initialized = false;

            if (!initialized) {
                instance.RegisterComponent<TransformComponent>("TransformComponent");
                instance.RegisterComponent<Rendering::CameraComponent>("CameraComponent");
                instance.RegisterComponent<Rendering::MaterialComponent>("MaterialComponent");
                instance.RegisterComponent<Rendering::MeshComponent>("MeshComponent");
                initialized = true;
            }

            return instance;
        }

        template<typename T>
        static void RegisterComponentType(const std::string& name) {
            Instance().RegisterComponent<T>(name);
        }
    };

    // Helper functions for entity serialization
    static void SerializeEntity(HE::Serialization::SerializationBackend& backend, entt::registry& registry, entt::entity entity) {
        auto& serializers = ComponentSerializers::Instance();

        backend.BeginObject("");

        // Serialize entity ID
        uint32_t entityId = static_cast<uint32_t>(entity);
        backend.Serialize("entity_id", entityId);

        // Serialize components as array: [ {"compId": xxx, "ComponentName": {...}}, ... ]
        backend.BeginArray("components");

        uint32_t componentIndex = 0;
        for (auto&& [id, storage] : registry.storage()) {
            if (storage.contains(entity)) {
                std::string componentName = serializers.GetComponentName(id);
                if (componentName.empty()) {
                    continue;
                }

                backend.BeginArrayElement(componentIndex++);
                backend.BeginObject("");
                backend.Serialize("compId", static_cast<uint32_t>(id));

                // Serialize component data with componentName - Serializer<T> will handle BeginObject/EndObject
                auto it = serializers.serializeFuncs.find(id);
                if (it != serializers.serializeFuncs.end()) {
                    it->second(backend, registry, entity, componentName);
                }

                backend.EndObject(); // array element
                backend.EndArrayElement();
            }
        }

        backend.EndArray();
        backend.EndObject();
    }

    static entt::entity DeserializeEntity(HE::Serialization::SerializationBackend& backend, Scene& scene) {
        auto& serializers = ComponentSerializers::Instance();
        auto& registry = scene.GetEntityManager().GetRegistry();

        backend.BeginObject("");

        uint32_t originalEntityId;
        backend.Deserialize("entity_id", originalEntityId);

        Entity entity = scene.GetEntityManager().CreateEntity();

        if (!backend.HasField("components")) {
            backend.EndObject();
            return entity;
        }

        uint32_t componentCount = backend.GetArraySize("components");
        backend.BeginArray("components");

        for (uint32_t i = 0; i < componentCount; ++i) {
            backend.BeginArrayElement(i);
            backend.BeginObject("");

            uint32_t compId = 0;
            if (!backend.Deserialize("compId", compId)) {
                backend.EndObject();
                backend.EndArrayElement();
                continue;
            }

            std::string componentName = serializers.GetComponentName(static_cast<entt::id_type>(compId));
            if (componentName.empty()) {
                backend.EndObject();
                backend.EndArrayElement();
                continue;
            }

            // Deserialize component data with componentName - Serializer<T> will handle BeginObject/EndObject
            if (backend.HasField(componentName)) {
                auto it = serializers.deserializeFuncs.find(static_cast<entt::id_type>(compId));
                if (it != serializers.deserializeFuncs.end()) {
                    it->second(backend, registry, static_cast<entt::entity>(entity), componentName);
                }
            }

            backend.EndObject();
            backend.EndArrayElement();
        }

        backend.EndArray();
        backend.EndObject();
        return entity;
    }

} // namespace HE

namespace HE::Serialization {

    void Serializer<Scene>::Serialize(SerializationBackend& backend, const std::string& name, const Scene& scene) {
        if (!name.empty())
            backend.BeginObject(name);

        // Scene metadata
        backend.Serialize("scene_name", scene.GetName().empty() ? "Untitled Scene" : scene.GetName());
        backend.Serialize("scene_version", 1);

        // Serialize entities
        backend.BeginArray("entities");

        auto& registry = const_cast<Scene&>(scene).GetEntityManager().GetRegistry();
        uint32_t entityIndex = 0;

        for (auto entity : registry.storage<entt::entity>()) {
            backend.BeginArrayElement(entityIndex++);
            HE::SerializeEntity(backend, registry, entity);
            backend.EndArrayElement();
        }

        backend.EndArray();

        if (!name.empty())
            backend.EndObject();
    }

    bool Serializer<Scene>::Deserialize(SerializationBackend& backend, const std::string& name, Scene& scene) {
        if (!name.empty())
            backend.BeginObject(name);

        // Clear existing scene
        auto& registry = scene.GetEntityManager().GetRegistry();
        std::vector<entt::entity> entities;
        for (auto entity : registry.storage<entt::entity>()) {
            entities.push_back(entity);
        }
        for (auto entity : entities) {
            registry.destroy(entity);
        }

        // Read scene metadata
        std::string sceneName;
        int sceneVersion;
        backend.Deserialize("scene_name", sceneName);
        backend.Deserialize("scene_version", sceneVersion);
        scene.SetName(sceneName);

        // Deserialize entities
        if (!backend.HasField("entities")) {
            if (!name.empty())
                backend.EndObject();
            return false;
        }

        uint32_t entityCount = backend.GetArraySize("entities");
        backend.BeginArray("entities");

        for (uint32_t i = 0; i < entityCount; ++i) {
            backend.BeginArrayElement(i);
            HE::DeserializeEntity(backend, scene);
            backend.EndArrayElement();
        }

        backend.EndArray();

        if (!name.empty())
            backend.EndObject();
        return true;
    }

} // namespace HE::Serialization
