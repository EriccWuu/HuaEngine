#pragma once

#include "Scene.h"
#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/Serialization/Serialization.h"
#include "HuaEngine/Serialization/SerializationManager.h"

namespace HE::Serialization {
    // Scene serialization template
    template<>
    struct Serializer<Scene> {
        static void Serialize(SerializationBackend& backend, const std::string& name, const Scene& scene);
        static bool Deserialize(SerializationBackend& backend, const std::string& name, Scene& scene);
    };

    // Convenience functions for Scene serialization
    inline bool SaveScene(Scene& scene, const std::string& filename, SerializationFormat format = SerializationFormat::YAML) {
        return SERIALIZE_TO_FILE(scene, filename, format);
    }

    inline bool LoadScene(const std::string& filename, Scene& scene, SerializationFormat format = SerializationFormat::YAML) {
        return DESERIALIZE_FROM_FILE(filename, scene, format);
    }

    // Pointer convenience functions (for compatibility)
    inline bool SaveScene(Scene* scene, const std::string& filename, SerializationFormat format = SerializationFormat::YAML) {
        if (!scene) return false;
        return SaveScene(*scene, filename, format);
    }

    inline bool LoadScene(const std::string& filename, Scene* scene, SerializationFormat format = SerializationFormat::YAML) {
        if (!scene) return false;
        return LoadScene(filename, *scene, format);
    }

} // namespace HE::Serialization

namespace HE {

    // Keep SaveScene/LoadScene in HE namespace for backward compatibility
    inline bool SaveScene(Scene* scene, const std::string& filename, HE::Serialization::SerializationFormat format = HE::Serialization::SerializationFormat::YAML) {
        return HE::Serialization::SaveScene(scene, filename, format);
    }

    inline bool LoadScene(Scene* scene, const std::string& filename, HE::Serialization::SerializationFormat format = HE::Serialization::SerializationFormat::YAML) {
        return HE::Serialization::LoadScene(filename, scene, format);
    }

} // namespace HE
