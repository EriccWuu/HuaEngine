#pragma once

// Main serialization headers
#include "Serialization.h"
#include "SerializationManager.h"
#include "ReflectionSerializer.h"

// Backend implementations
#include "JsonSerializationBackend.h"
// #include "YamlSerializationBackend.h" // Uncomment when YAML-cpp is available

// Scene serialization
#include "SceneSerializer.h"
#include "HuaEngine/Rendering/Material/MaterialSerialization.h"

namespace HE {

    // Initialize the serialization system - call this during engine startup
    void InitializeSerialization();

    // Convenience functions for quick serialization

    // Serialize any object to JSON string
    template<typename T>
    std::string ToJson(const T& object) {
        return SerializationManager::Instance().SerializeToString(object, SerializationFormat::JSON);
    }

    // Deserialize any object from JSON string
    template<typename T>
    bool FromJson(const std::string& jsonString, T& object) {
        return SerializationManager::Instance().DeserializeFromString(jsonString, object, SerializationFormat::JSON);
    }

    // Serialize any object to JSON file
    template<typename T>
    bool SaveAsJson(const T& object, const std::string& filename) {
        return SerializationManager::Instance().SerializeToFile(object, filename, SerializationFormat::JSON);
    }

    // Deserialize any object from JSON file
    template<typename T>
    bool LoadFromJson(const std::string& filename, T& object) {
        return SerializationManager::Instance().DeserializeFromFile(filename, object, SerializationFormat::JSON);
    }

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
