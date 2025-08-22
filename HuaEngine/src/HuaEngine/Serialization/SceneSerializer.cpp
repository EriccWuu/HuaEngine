#include "enginepch.h"
#include "SceneSerializer.h"
#include "Module/Rendering/RenderingComponent.h"

namespace HE {

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
                
                // Serialize component type ID
                backend.Serialize("component_type_id", static_cast<uint32_t>(id));
                
                // Serialize component data based on type
                SerializeComponentData(backend, entity, id);
                
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

            uint32_t componentTypeId;
            backend.Deserialize("component_type_id", componentTypeId);

            // Deserialize component data based on type
            DeserializeComponentData(backend, entity, static_cast<entt::id_type>(componentTypeId));

            backend.EndArrayElement();
        }

        backend.EndObject();
        return entity;
    }

    void SceneSerializer::SerializeComponentData(SerializationBackend& backend, entt::entity entity, entt::id_type componentTypeId) {
        auto& registry = m_Scene->GetEntityManager().GetRegistry();
        
        // Get component type hash for comparison
        if (componentTypeId == entt::type_hash<TransformComponent>::value()) {
            auto& component = registry.get<TransformComponent>(entity);
            backend.BeginObject("data");
            Serializer<TransformComponent>::Serialize(backend, "", component);
            backend.EndObject();
        }
        else if (componentTypeId == entt::type_hash<CameraComponent>::value()) {
            auto& component = registry.get<CameraComponent>(entity);
            backend.BeginObject("data");
            Serializer<CameraComponent>::Serialize(backend, "", component);
            backend.EndObject();
        }
        else if (componentTypeId == entt::type_hash<RendererComponent>::value()) {
            auto& component = registry.get<RendererComponent>(entity);
            backend.BeginObject("data");
            // Note: RendererComponent contains Ref<> objects that cannot be directly serialized
            // For now, we'll serialize placeholder data
            backend.Serialize("shader_name", "default_shader");
            backend.Serialize("texture_name", "default_texture");
            backend.EndObject();
        }
        else if (componentTypeId == entt::type_hash<MeshComponent>::value()) {
            auto& component = registry.get<MeshComponent>(entity);
            backend.BeginObject("data");
            // Note: MeshComponent contains Ref<> objects that cannot be directly serialized
            // For now, we'll serialize placeholder data
            backend.Serialize("vertex_array_name", "default_mesh");
            backend.EndObject();
        }
        else if (componentTypeId == entt::type_hash<NativeScriptComponent>::value()) {
            // NativeScriptComponent contains function pointers that cannot be serialized
            backend.BeginObject("data");
            backend.Serialize("script_type", "native_script");
            backend.EndObject();
        }
        // Add more component types here as needed
        else {
            // Unknown component type, just create empty data
            backend.BeginObject("data");
            backend.EndObject();
        }
    }

    void SceneSerializer::DeserializeComponentData(SerializationBackend& backend, Entity& entity, entt::id_type componentTypeId) {
        auto& registry = m_Scene->GetEntityManager().GetRegistry();
        auto entityHandle = static_cast<entt::entity>(entity);
        
        // Get component type hash for comparison
        if (componentTypeId == entt::type_hash<TransformComponent>::value()) {
            TransformComponent component;
            if (backend.HasField("data")) {
                backend.BeginObject("data");
                Serializer<TransformComponent>::Deserialize(backend, "", component);
                backend.EndObject();
            }
            registry.emplace<TransformComponent>(entityHandle, component);
        }
        else if (componentTypeId == entt::type_hash<CameraComponent>::value()) {
            CameraComponent component;
            if (backend.HasField("data")) {
                backend.BeginObject("data");
                Serializer<CameraComponent>::Deserialize(backend, "", component);
                backend.EndObject();
            }
            registry.emplace<CameraComponent>(entityHandle, component);
        }
        else if (componentTypeId == entt::type_hash<RendererComponent>::value()) {
            RendererComponent component;
            if (backend.HasField("data")) {
                backend.BeginObject("data");
                // Note: For now, we just create a default component
                // In a real implementation, you would load the actual resources
                std::string shaderName, textureName;
                backend.Deserialize("shader_name", shaderName);
                backend.Deserialize("texture_name", textureName);
                backend.EndObject();
            }
            registry.emplace<RendererComponent>(entityHandle, component);
        }
        else if (componentTypeId == entt::type_hash<MeshComponent>::value()) {
            MeshComponent component;
            if (backend.HasField("data")) {
                backend.BeginObject("data");
                // Note: For now, we just create a default component
                // In a real implementation, you would load the actual mesh
                std::string meshName;
                backend.Deserialize("vertex_array_name", meshName);
                backend.EndObject();
            }
            registry.emplace<MeshComponent>(entityHandle, component);
        }
        else if (componentTypeId == entt::type_hash<NativeScriptComponent>::value()) {
            NativeScriptComponent component;
            if (backend.HasField("data")) {
                backend.BeginObject("data");
                std::string scriptType;
                backend.Deserialize("script_type", scriptType);
                backend.EndObject();
            }
            registry.emplace<NativeScriptComponent>(entityHandle, component);
        }
        // Add more component types here as needed
    }

}
