#include "enginepch.h"
#include "SceneSerializer.h"
#include "Module/Rendering/RenderingComponent.h"
#include <functional>
#include <unordered_map>
#include <type_traits>
#include "HuaEngine/Math/Math.h"

namespace HE {

    // Component serialization registry
    struct ComponentSerializers {
        using SerializeFunc = std::function<void(HE::Serialization::SerializationBackend&, entt::registry&, entt::entity)>;
        using DeserializeFunc = std::function<void(HE::Serialization::SerializationBackend&, entt::registry&, entt::entity)>;
        
        std::unordered_map<entt::id_type, SerializeFunc> serializeFuncs;
        std::unordered_map<entt::id_type, DeserializeFunc> deserializeFuncs;
        
        template<typename T>
        void RegisterComponent() {
            auto typeId = entt::type_hash<T>::value();
            
            // Register serialization function
            serializeFuncs[typeId] = [](HE::Serialization::SerializationBackend& backend, entt::registry& registry, entt::entity entity) {
                if (registry.all_of<T>(entity)) {
                    auto& component = registry.get<T>(entity);
                    HE::Serialization::SerializeValue(backend, "data", component);
                }
            };
            
            // Register deserialization function
            deserializeFuncs[typeId] = [](HE::Serialization::SerializationBackend& backend, entt::registry& registry, entt::entity entity) {
                if (backend.HasField("data")) {
                    T component{};
                    if (HE::Serialization::DeserializeValue(backend, "data", component)) {
                        if constexpr (std::is_same_v<T, TransformComponent>) {
                            std::cout << component.Position << std::endl;
                        }
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
                instance.RegisterComponent<Rendering::CameraComponent>();
                instance.RegisterComponent<Rendering::MaterialComponent>();
                instance.RegisterComponent<Rendering::MeshComponent>();
                
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
                backend->BeginArrayElement(entityIndex++);
                SerializeEntity(*backend, entity);
                backend->EndArrayElement();
            }

            backend->EndArray();
            backend->EndObject();

            return backend->SaveToString();
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
            HE_CORE_INFO("Deserializing {0} entities from scene file", entityCount);
            std::vector<entt::entity> deserializedEntities;
            deserializedEntities.reserve(entityCount);
            
            for (uint32_t i = 0; i < entityCount; ++i) {
                HE_CORE_TRACE("Deserializing entity {0}/{1} from file", i, entityCount);
                backend->BeginArrayElement(i);
                entt::entity entity = DeserializeEntity(*backend);
                deserializedEntities.push_back(entity);
                backend->EndArrayElement();
            }

            HE_CORE_INFO("Successfully deserialized scene with {0} entities", deserializedEntities.size());

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
            backend->Deserialize("scene_name", sceneName);
            backend->Deserialize("scene_version", sceneVersion);

            // Deserialize entities
            uint32_t entityCount = backend->GetArraySize("entities");
            HE_CORE_INFO("Deserializing {0} entities from scene string", entityCount);
            std::vector<entt::entity> deserializedEntities;
            deserializedEntities.reserve(entityCount);
            
            for (uint32_t i = 0; i < entityCount; ++i) {
                HE_CORE_TRACE("Deserializing entity {0}/{1} from string", i, entityCount);
                backend->BeginArrayElement(i);
                entt::entity entity = DeserializeEntity(*backend);
                deserializedEntities.push_back(entity);
                backend->EndArrayElement();
            }

            HE_CORE_INFO("Successfully deserialized scene from string with {0} entities", deserializedEntities.size());

            return true;
        } catch (const std::exception& e) {
            HE_CORE_ERROR("Failed to deserialize scene from string: {0}", e.what());
            return false;
        }
    }

    void SceneSerializer::SerializeEntity(HE::Serialization::SerializationBackend& backend, entt::entity entity) {
        auto& registry = m_Scene->GetEntityManager().GetRegistry();
        
        // Create entity object wrapper
        backend.BeginObject("");
        
        // Serialize entity ID
        uint32_t entityId = static_cast<uint32_t>(entity);
        backend.Serialize("entity_id", entityId);
        HE_CORE_TRACE("Serializing entity ID: {0}", entityId);

        // Serialize components using reflection system
        backend.BeginArray("components");
        uint32_t componentIndex = 0;

        // Get all component types for this entity
        for (auto&& [id, storage] : registry.storage()) {
            if (storage.contains(entity)) {
                HE_CORE_TRACE("Serializing component type ID: {0} for entity {1}", static_cast<uint32_t>(id), entityId);
                backend.BeginArrayElement(componentIndex++);

                // backend.BeginObject("");
                
                // Serialize component type ID
                backend.Serialize("component_type_id", static_cast<uint32_t>(id));
                
                // Serialize component data based on type
                SerializeComponentData(backend, entity, id);

                // backend.EndObject();
                
                backend.EndArrayElement();
            }
        }

        HE_CORE_TRACE("Serialized {0} components for entity {1}", componentIndex, entityId);
        backend.EndArray();
        backend.EndObject();
    }

    entt::entity SceneSerializer::DeserializeEntity(HE::Serialization::SerializationBackend& backend) {
        // 在BeginArrayElement之后，我们需要开始处理实体对象
        backend.BeginObject("");
        
        // Deserialize entity ID (for reference, but we'll create a new entity)
        uint32_t originalEntityId;
        if (!backend.Deserialize("entity_id", originalEntityId)) {
            HE_CORE_ERROR("Failed to deserialize entity_id");
            originalEntityId = 0;
        }
        
        HE_CORE_TRACE("Deserializing entity: original ID={0}", originalEntityId);

        // Create new entity
        Entity entity = m_Scene->GetEntityManager().CreateEntity();
        
        HE_CORE_TRACE("Created new entity with handle={0}", entity.GetUid());

        // Deserialize components - 检查components数组是否存在
        if (!backend.HasField("components")) {
            HE_CORE_WARN("Entity {0} has no components field", originalEntityId);
            backend.EndObject();
            return entity;
        }
        
        uint32_t componentCount = backend.GetArraySize("components");
        HE_CORE_TRACE("Component count for entity {0}: {1}", entity.GetUid(), componentCount);
        
        for (uint32_t i = 0; i < componentCount; ++i) {
            HE_CORE_TRACE("Processing component {0}/{1}", i, componentCount);
            backend.BeginArrayElement(i);

            backend.BeginObject("");

            uint32_t componentTypeId;
            if (!backend.Deserialize("component_type_id", componentTypeId)) {
                HE_CORE_ERROR("Failed to deserialize component_type_id for component {0}", i);
                backend.EndObject();
                backend.EndArrayElement();
                continue;
            }
            
            HE_CORE_TRACE("Deserializing component type ID: {0}", componentTypeId);

            // Deserialize component data based on type
            DeserializeComponentData(backend, entity, static_cast<entt::id_type>(componentTypeId));

            backend.EndObject();
            backend.EndArrayElement();
        }

        backend.EndObject();
        return entity;
    }

    void SceneSerializer::SerializeComponentData(HE::Serialization::SerializationBackend& backend, entt::entity entity, entt::id_type componentTypeId) {
        auto& registry = m_Scene->GetEntityManager().GetRegistry();
        auto& serializers = ComponentSerializers::Instance();
        
        // Try to use registered serializer first
        auto it = serializers.serializeFuncs.find(componentTypeId);
        if (it != serializers.serializeFuncs.end()) {
            HE_CORE_TRACE("Found serializer for component type ID: {0}", static_cast<uint32_t>(componentTypeId));
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
            HE_CORE_TRACE("Found deserializer for component type ID: {0}", static_cast<uint32_t>(componentTypeId));
            it->second(backend, registry, entityHandle);
            return;
        } else {
            HE_CORE_WARN("No deserializer found for component type ID: {0}", static_cast<uint32_t>(componentTypeId));
        }
    }

}
