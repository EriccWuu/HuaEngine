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
        using SerializeFunc = std::function<void(HE::Serialization::SerializationBackend&, entt::registry&, entt::entity)>;
        using DeserializeFunc = std::function<void(HE::Serialization::SerializationBackend&, entt::registry&, entt::entity)>;

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

            // Register serialization function - uses Serializer with typed field format
            serializeFuncs[typeId] = [](HE::Serialization::SerializationBackend& backend, entt::registry& registry, entt::entity entity) {
                if (registry.all_of<T>(entity)) {
                    auto& component = registry.get<T>(entity);
                    HE::Serialization::Serializer<T>::Serialize(backend, "", component);
                }
            };

            // Register deserialization function
            deserializeFuncs[typeId] = [typeId](HE::Serialization::SerializationBackend& backend, entt::registry& registry, entt::entity entity) {
                T component{};
                if (HE::Serialization::Serializer<T>::Deserialize(backend, "", component)) {
                    registry.emplace_or_replace<T>(entity, std::move(component));
                }
                else {
                    HE_CORE_WARN("Failed to deserialize component {0}", typeId);
                }
            };
        }

        // Get component name from type ID
        std::string GetComponentName(entt::id_type typeId) const {
            auto it = typeIdToName.find(typeId);
            return it != typeIdToName.end() ? it->second : "";
        }

        // Get type ID from component name
        entt::id_type GetTypeId(const std::string& name) const {
            auto it = nameToTypeId.find(name);
            return it != nameToTypeId.end() ? it->second : 0;
        }

        static ComponentSerializers& Instance() {
            static ComponentSerializers instance;
            static bool initialized = false;

            if (!initialized) {
                // Register all known component types with their names
                instance.RegisterComponent<TransformComponent>("TransformComponent");
                instance.RegisterComponent<Rendering::CameraComponent>("CameraComponent");
                instance.RegisterComponent<Rendering::MaterialComponent>("MaterialComponent");
                instance.RegisterComponent<Rendering::MeshComponent>("MeshComponent");

                initialized = true;
            }

            return instance;
        }

        // Helper function to register additional components at runtime
        template<typename T>
        static void RegisterComponentType(const std::string& name) {
            Instance().RegisterComponent<T>(name);
        }
    };

    // Convenience macro for registering components
    #define REGISTER_COMPONENT_SERIALIZER(ComponentType) \
        ComponentSerializers::RegisterComponentType<ComponentType>(#ComponentType);

    bool SceneSerializer::SerializeScene(const std::string& filename, HE::Serialization::SerializationFormat format) {
        try {
            auto backend = HE::Serialization::SerializationManager::Instance().CreateBackend(format);
            if (!backend) {
                HE_CORE_ERROR("Failed to create serialization backend for format");
                return false;
            }

            backend->Reset();
            backend->BeginObject(); // Root object

            // Scene metadata
            backend->Serialize("scene_name", "Untitled Scene");
            backend->Serialize("scene_version", 1);

            // Serialize entities
            backend->BeginArray("entities");
            
            auto& registry = m_Scene->GetEntityManager().GetRegistry();
            uint32_t entityIndex = 0;
            
            // Use storage to iterate over all entities
            for (auto entity : registry.storage<entt::entity>()) {
                backend->BeginArrayElement(entityIndex++);
                SerializeEntity(*backend, entity);
                backend->EndArrayElement();
            }

            backend->EndArray();
            backend->EndObject();

            backend->SaveToFile(filename);
            return true;
        } catch (const std::exception& e) {
            HE_CORE_ERROR("Failed to serialize scene: {0}", e.what());
            return false;
        }
    }

    std::string SceneSerializer::SerializeSceneToString(HE::Serialization::SerializationFormat format) {
        try {
            auto backend = HE::Serialization::SerializationManager::Instance().CreateBackend(format);
            if (!backend) {
                HE_CORE_ERROR("Failed to create serialization backend for format");
                return "";
            }

            backend->Reset();
            backend->BeginObject(); // Root object

            // Scene metadata
            backend->Serialize("scene_name", "Untitled Scene");
            backend->Serialize("scene_version", 1);

            // Serialize entities
            backend->BeginArray("entities");
            
            auto& registry = m_Scene->GetEntityManager().GetRegistry();
            uint32_t entityIndex = 0;
            
            // Use storage to iterate over all entities
            for (auto entity : registry.storage<entt::entity>()) {
                HE_CORE_TRACE("Serializing entity index {0}, entity ID {1}", entityIndex, static_cast<uint32_t>(entity));
                backend->BeginArrayElement(entityIndex++);
                SerializeEntity(*backend, entity);
                backend->EndArrayElement();
            }

            backend->EndArray();
            backend->EndObject();

            std::string result = backend->SaveToString();
            return result;
        } catch (const std::exception& e) {
            HE_CORE_ERROR("Failed to serialize scene to string: {0}", e.what());
            return "";
        }
    }

    bool SceneSerializer::DeserializeScene(const std::string& filename, HE::Serialization::SerializationFormat format) {
        try {
            auto backend = HE::Serialization::SerializationManager::Instance().CreateBackend(format);
            if (!backend) {
                HE_CORE_ERROR("Failed to create serialization backend for format");
                return false;
            }

            backend->LoadFromFile(filename);

            // Clear existing scene - destroy entities one by one
            auto& registry = m_Scene->GetEntityManager().GetRegistry();
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
            backend->Deserialize("scene_name", sceneName);
            backend->Deserialize("scene_version", sceneVersion);

            // Deserialize entities
            uint32_t entityCount = backend->GetArraySize("entities");
            std::vector<entt::entity> deserializedEntities;
            deserializedEntities.reserve(entityCount);
            
            // Begin processing entities array
            backend->BeginArray("entities");
            
            for (uint32_t i = 0; i < entityCount; ++i) {
                backend->BeginArrayElement(i);
                entt::entity entity = DeserializeEntity(*backend);
                deserializedEntities.push_back(entity);
                backend->EndArrayElement();
            }
            
            backend->EndArray(); // End entities array

            return true;
        } catch (const std::exception& e) {
            HE_CORE_ERROR("Failed to deserialize scene: {0}", e.what());
            return false;
        }
    }

    bool SceneSerializer::DeserializeSceneFromString(const std::string& data, HE::Serialization::SerializationFormat format) {
        try {
            auto backend = HE::Serialization::SerializationManager::Instance().CreateBackend(format);
            if (!backend) {
                HE_CORE_ERROR("Failed to create serialization backend for format");
                return false;
            }

            backend->LoadFromString(data);

            // Clear existing scene - destroy entities one by one
            auto& registry = m_Scene->GetEntityManager().GetRegistry();
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
            bool hasSceneName = backend->Deserialize("scene_name", sceneName);
            bool hasSceneVersion = backend->Deserialize("scene_version", sceneVersion);

            // Check if entities field exists
            if (!backend->HasField("entities")) {
                HE_CORE_ERROR("No 'entities' field found in JSON data");
            }

            // Deserialize entities
            uint32_t entityCount = backend->GetArraySize("entities");
            std::vector<entt::entity> deserializedEntities;
            deserializedEntities.reserve(entityCount);
            
            // Begin processing entities array
            backend->BeginArray("entities");
            
            for (uint32_t i = 0; i < entityCount; ++i) {
                backend->BeginArrayElement(i);
                entt::entity entity = DeserializeEntity(*backend);
                deserializedEntities.push_back(entity);
                backend->EndArrayElement();
            }
            
            backend->EndArray(); // End entities array

            return true;
        } catch (const std::exception& e) {
            HE_CORE_ERROR("Failed to deserialize scene from string: {0}", e.what());
            return false;
        }
    }

    void SceneSerializer::SerializeEntity(HE::Serialization::SerializationBackend& backend, entt::entity entity) {
        auto& registry = m_Scene->GetEntityManager().GetRegistry();
        auto& serializers = ComponentSerializers::Instance();

        // Create entity object wrapper
        backend.BeginObject("");

        // Serialize entity ID
        uint32_t entityId = static_cast<uint32_t>(entity);
        backend.Serialize("entity_id", entityId);

        // Serialize components as array
        // Format: "components": [ {"compId": xxx, "ComponentName": {...}}, ... ]
        backend.BeginArray("components");

        uint32_t componentIndex = 0;
        // Get all component types for this entity
        for (auto&& [id, storage] : registry.storage()) {
            if (storage.contains(entity)) {
                // Get component name from registry
                std::string componentName = serializers.GetComponentName(id);
                if (componentName.empty()) {
                    HE_CORE_WARN("Unknown component type ID: {0}, skipping serialization", static_cast<uint32_t>(id));
                    continue;
                }

                // Serialize component in array format
                // Format: {"compId": xxx, "ComponentName": {...fields...}}
                backend.BeginArrayElement(componentIndex++);
                backend.BeginObject("");
                backend.Serialize("compId", static_cast<uint32_t>(id));
                backend.BeginObject(componentName);
                SerializeComponentData(backend, entity, id);
                backend.EndObject(); // End component data object
                backend.EndObject(); // End array element object
                backend.EndArrayElement();
            }
        }

        backend.EndArray(); // End components array
        backend.EndObject(); // End entity object
    }

    entt::entity SceneSerializer::DeserializeEntity(HE::Serialization::SerializationBackend& backend) {
        auto& serializers = ComponentSerializers::Instance();

        // Begin processing entity object
        backend.BeginObject("");

        // Deserialize entity ID (for reference, but we'll create a new entity)
        uint32_t originalEntityId;
        if (!backend.Deserialize("entity_id", originalEntityId)) {
            HE_CORE_ERROR("Failed to deserialize entity_id");
        }

        // Create new entity
        Entity entity = m_Scene->GetEntityManager().CreateEntity();

        // Deserialize components - check if components array exists
        if (!backend.HasField("components")) {
            HE_CORE_WARN("Entity {0} has no components field", originalEntityId);
            backend.EndObject();
            return entity;
        }

        // Get component count and process array
        uint32_t componentCount = backend.GetArraySize("components");
        backend.BeginArray("components");

        for (uint32_t i = 0; i < componentCount; ++i) {
            backend.BeginArrayElement(i);
            backend.BeginObject("");

            // Read compId
            uint32_t compId = 0;
            if (!backend.Deserialize("compId", compId)) {
                HE_CORE_WARN("Failed to read compId for component {0}", i);
                backend.EndObject();
                backend.EndArrayElement();
                continue;
            }

            // Get component name from compId
            std::string componentName = serializers.GetComponentName(static_cast<entt::id_type>(compId));
            if (componentName.empty()) {
                HE_CORE_WARN("Unknown component ID: {0}, skipping deserialization", compId);
                backend.EndObject();
                backend.EndArrayElement();
                continue;
            }

            // Deserialize component data from the named object
            if (backend.HasField(componentName)) {
                backend.BeginObject(componentName);
                DeserializeComponentData(backend, entity, static_cast<entt::id_type>(compId));
                backend.EndObject();
            }

            backend.EndObject();
            backend.EndArrayElement();
        }

        backend.EndArray(); // End components array
        backend.EndObject(); // End entity object
        return entity;
    }

    void SceneSerializer::SerializeComponentData(HE::Serialization::SerializationBackend& backend, entt::entity entity, entt::id_type componentTypeId) {
        auto& registry = m_Scene->GetEntityManager().GetRegistry();
        auto& serializers = ComponentSerializers::Instance();
        
        // Try to use registered serializer first
        auto it = serializers.serializeFuncs.find(componentTypeId);
        if (it != serializers.serializeFuncs.end()) {
            it->second(backend, registry, entity);
            return;
        } else {
            HE_CORE_WARN("No serializer found for component type ID: {0}", static_cast<uint32_t>(componentTypeId));
        }
    }

    void SceneSerializer::DeserializeComponentData(HE::Serialization::SerializationBackend& backend, Entity& entity, entt::id_type componentTypeId) {
        auto& registry = m_Scene->GetEntityManager().GetRegistry();
        auto entityHandle = static_cast<entt::entity>(entity);
        auto& serializers = ComponentSerializers::Instance();
        
        // Try to use registered deserializer first
        auto it = serializers.deserializeFuncs.find(componentTypeId);
        if (it != serializers.deserializeFuncs.end()) {
            it->second(backend, registry, entityHandle);
            return;
        } else {
            HE_CORE_WARN("No deserializer found for component type ID: {0}", static_cast<uint32_t>(componentTypeId));
        }
    }

}
