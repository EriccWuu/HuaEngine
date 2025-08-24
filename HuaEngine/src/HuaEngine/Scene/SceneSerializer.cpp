#include "enginepch.h"
#include "SceneSerializer.h"
#include "Module/Rendering/RenderingComponent.h"
#include <functional>
#include <unordered_map>

namespace HE {

    // Component serialization registry
    struct ComponentSerializers {
        using SerializeFunc = std::function<void(SerializationBackend&, entt::registry&, entt::entity)>;
        using DeserializeFunc = std::function<void(SerializationBackend&, entt::registry&, entt::entity)>;
        
        std::unordered_map<entt::id_type, SerializeFunc> serializeFuncs;
        std::unordered_map<entt::id_type, DeserializeFunc> deserializeFuncs;
        
        template<typename T>
        void RegisterComponent() {
            auto typeId = entt::type_hash<T>::value();
            
            // Register serialization function
            serializeFuncs[typeId] = [](SerializationBackend& backend, entt::registry& registry, entt::entity entity) {
                if (registry.all_of<T>(entity)) {
                    auto& component = registry.get<T>(entity);
                    SerializeValue(backend, "data", component);
                }
            };
            
            // Register deserialization function
            deserializeFuncs[typeId] = [](SerializationBackend& backend, entt::registry& registry, entt::entity entity) {
                if (backend.HasField("data")) {
                    T component{};
                    if (DeserializeValue(backend, "data", component)) {
                        registry.emplace_or_replace<T>(entity, std::move(component));
                    }
                }
            };
        }
        
        static ComponentSerializers& Instance() {
            static ComponentSerializers instance;
            static bool initialized = false;
            
            if (!initialized) {
                // Register all known component types that can be automatically serialized
                instance.RegisterComponent<TransformComponent>();
                instance.RegisterComponent<CameraComponent>();
                instance.RegisterComponent<MaterialComponent>();
                instance.RegisterComponent<MeshComponent>();
                
                initialized = true;
            }
            
            return instance;
        }
        
        // Helper function to register additional components at runtime
        template<typename T>
        static void RegisterComponentType() {
            Instance().RegisterComponent<T>();
        }
    };
    
    // Convenience macro for registering components
    #define REGISTER_COMPONENT_SERIALIZER(ComponentType) \
        ComponentSerializers::RegisterComponentType<ComponentType>();

    bool SceneSerializer::SerializeScene(const std::string& filename, SerializationFormat format) {
        try {
            auto backend = SerializationManager::Instance().CreateBackend(format);
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

    bool SceneSerializer::DeserializeScene(const std::string& filename, SerializationFormat format) {
        try {
            auto backend = SerializationManager::Instance().CreateBackend(format);
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
            for (uint32_t i = 0; i < entityCount; ++i) {
                backend->BeginArrayElement(i);
                DeserializeEntity(*backend);
                backend->EndArrayElement();
            }

            return true;
        } catch (const std::exception& e) {
            HE_CORE_ERROR("Failed to deserialize scene: {0}", e.what());
            return false;
        }
    }

    void SceneSerializer::SerializeEntity(SerializationBackend& backend, entt::entity entity) {
        auto& registry = m_Scene->GetEntityManager().GetRegistry();
        
        // Create entity object wrapper
        backend.BeginObject("");
        
        // Serialize entity ID
        uint32_t entityId = static_cast<uint32_t>(entity);
        backend.Serialize("entity_id", entityId);

        // Serialize components using reflection system
        backend.BeginArray("components");
        uint32_t componentIndex = 0;

        // Get all component types for this entity
        for (auto&& [id, storage] : registry.storage()) {
            if (storage.contains(entity)) {
                backend.BeginArrayElement(componentIndex++);

                backend.BeginObject("");
                
                // Serialize component type ID
                backend.Serialize("component_type_id", static_cast<uint32_t>(id));
                
                // Serialize component data based on type
                SerializeComponentData(backend, entity, id);

                backend.EndObject();
                
                backend.EndArrayElement();
            }
        }

        backend.EndArray();
        backend.EndObject();
    }

    entt::entity SceneSerializer::DeserializeEntity(SerializationBackend& backend) {
        // Begin entity object wrapper
        backend.BeginObject("");
        
        // Deserialize entity ID
        uint32_t entityId;
        backend.Deserialize("entity_id", entityId);

        // Create entity
        Entity entity = m_Scene->GetEntityManager().CreateEntity();

        // Deserialize components
        uint32_t componentCount = backend.GetArraySize("components");
        for (uint32_t i = 0; i < componentCount; ++i) {
            backend.BeginArrayElement(i);

            backend.BeginObject("");

            uint32_t componentTypeId;
            backend.Deserialize("component_type_id", componentTypeId);

            // Deserialize component data based on type
            DeserializeComponentData(backend, entity, static_cast<entt::id_type>(componentTypeId));

            backend.EndObject();

            backend.EndArrayElement();
        }

        backend.EndObject();
        return entity;
    }

    void SceneSerializer::SerializeComponentData(SerializationBackend& backend, entt::entity entity, entt::id_type componentTypeId) {
        auto& registry = m_Scene->GetEntityManager().GetRegistry();
        auto& serializers = ComponentSerializers::Instance();
        
        // Try to use registered serializer first
        auto it = serializers.serializeFuncs.find(componentTypeId);
        if (it != serializers.serializeFuncs.end()) {
            it->second(backend, registry, entity);
            return;
        }
    }

    void SceneSerializer::DeserializeComponentData(SerializationBackend& backend, Entity& entity, entt::id_type componentTypeId) {
        auto& registry = m_Scene->GetEntityManager().GetRegistry();
        auto entityHandle = static_cast<entt::entity>(entity);
        auto& serializers = ComponentSerializers::Instance();
        
        // Try to use registered deserializer first
        auto it = serializers.deserializeFuncs.find(componentTypeId);
        if (it != serializers.deserializeFuncs.end()) {
            it->second(backend, registry, entityHandle);
            return;
        }
    }

}
