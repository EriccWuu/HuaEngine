#pragma once

#include "Scene.h"
#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/Serialization/Serialization.h"
#include "HuaEngine/Serialization/SerializationManager.h"
#include "entt.hpp"

namespace HE {

    class SceneSerializer {
    public:
        SceneSerializer(Scene* scene) : m_Scene(scene) {}

        // Serialize entire scene
        bool SerializeScene(const std::string& filename, HE::Serialization::SerializationFormat format = HE::Serialization::SerializationFormat::JSON);
        bool DeserializeScene(const std::string& filename, HE::Serialization::SerializationFormat format = HE::Serialization::SerializationFormat::JSON);

        // Serialize to string
        std::string SerializeSceneToString(HE::Serialization::SerializationFormat format = HE::Serialization::SerializationFormat::JSON);
        bool DeserializeSceneFromString(const std::string& data, HE::Serialization::SerializationFormat format = HE::Serialization::SerializationFormat::JSON);

        // Serialize individual entities
        void SerializeEntity(HE::Serialization::SerializationBackend& backend, entt::entity entity);
        entt::entity DeserializeEntity(HE::Serialization::SerializationBackend& backend);

    private:
        Scene* m_Scene;

        // Component data serialization methods
        void SerializeComponentData(HE::Serialization::SerializationBackend& backend, entt::entity entity, entt::id_type componentTypeId);
        void DeserializeComponentData(HE::Serialization::SerializationBackend& backend, Entity& entity, entt::id_type componentTypeId);

        void SerializeComponents(HE::Serialization::SerializationBackend& backend, entt::entity entity);
        void DeserializeComponents(HE::Serialization::SerializationBackend& backend, entt::entity entity);

        // Component serialization helpers
        template<typename T>
        void SerializeComponent(HE::Serialization::SerializationBackend& backend, entt::entity entity, const std::string& componentName) {
            auto& registry = m_Scene->GetEntityManager().GetRegistry();
            if (registry.all_of<T>(entity)) {
                const T& component = registry.get<T>(entity);
                HE::Serialization::Serializer<T>::Serialize(backend, componentName, component);
            }
        }

        template<typename T>
        void DeserializeComponent(HE::Serialization::SerializationBackend& backend, entt::entity entity, const std::string& componentName) {
            if (backend.HasField(componentName)) {
                auto& registry = m_Scene->GetEntityManager().GetRegistry();
                T component{};
                if (HE::Serialization::Serializer<T>::Deserialize(backend, componentName, component)) {
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
    inline bool SaveScene(Scene* scene, const std::string& filename, HE::Serialization::SerializationFormat format = HE::Serialization::SerializationFormat::JSON) {
        SceneSerializer serializer(scene);
        return serializer.SerializeScene(filename, format);
    }

    inline bool LoadScene(Scene* scene, const std::string& filename, HE::Serialization::SerializationFormat format = HE::Serialization::SerializationFormat::JSON) {
        SceneSerializer serializer(scene);
        return serializer.DeserializeScene(filename, format);
    }
}
