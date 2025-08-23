#pragma once

#include "Scene.h"
#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/Serialization/SerializationCore.h"
#include "HuaEngine/Serialization/SerializationManager.h"
#include "entt.hpp"

namespace HE {

    class SceneSerializer {
    public:
        SceneSerializer(Scene* scene) : m_Scene(scene) {}

        // Serialize entire scene
        bool SerializeScene(const std::string& filename, SerializationFormat format = SerializationFormat::JSON);
        bool DeserializeScene(const std::string& filename, SerializationFormat format = SerializationFormat::JSON);

        // Serialize to string
        std::string SerializeSceneToString(SerializationFormat format = SerializationFormat::JSON);
        bool DeserializeSceneFromString(const std::string& data, SerializationFormat format = SerializationFormat::JSON);

        // Serialize individual entities
        void SerializeEntity(SerializationBackend& backend, entt::entity entity);
        entt::entity DeserializeEntity(SerializationBackend& backend);

    private:
        Scene* m_Scene;

        // Component data serialization methods
        void SerializeComponentData(SerializationBackend& backend, entt::entity entity, entt::id_type componentTypeId);
        void DeserializeComponentData(SerializationBackend& backend, Entity& entity, entt::id_type componentTypeId);

        void SerializeComponents(SerializationBackend& backend, entt::entity entity);
        void DeserializeComponents(SerializationBackend& backend, entt::entity entity);

        // Component serialization helpers
        template<typename T>
        void SerializeComponent(SerializationBackend& backend, entt::entity entity, const std::string& componentName) {
            auto& registry = m_Scene->GetEntityManager().GetRegistry();
            if (registry.all_of<T>(entity)) {
                const T& component = registry.get<T>(entity);
                Serializer<T>::Serialize(backend, componentName, component);
            }
        }

        template<typename T>
        void DeserializeComponent(SerializationBackend& backend, entt::entity entity, const std::string& componentName) {
            if (backend.HasField(componentName)) {
                auto& registry = m_Scene->GetEntityManager().GetRegistry();
                T component{};
                if (Serializer<T>::Deserialize(backend, componentName, component)) {
                    registry.emplace_or_replace<T>(entity, std::move(component));
                }
            }
        }
    };

    // Entity data structure for serialization
    struct EntityData {
        uint32_t id;
        std::string name;
        bool active = true;
    };

    // Scene-specific convenience functions
    inline bool SaveScene(Scene* scene, const std::string& filename, SerializationFormat format = SerializationFormat::JSON) {
        SceneSerializer serializer(scene);
        return serializer.SerializeScene(filename, format);
    }

    inline bool LoadScene(Scene* scene, const std::string& filename, SerializationFormat format = SerializationFormat::JSON) {
        SceneSerializer serializer(scene);
        return serializer.DeserializeScene(filename, format);
    }
}
