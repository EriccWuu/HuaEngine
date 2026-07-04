#pragma once

// Main serialization headers
#include "SerializationCore.h"
#include "SerializationManager.h"

// Backend implementations
#include "JsonSerializationBackend.h"
#include "YamlSerializationBackend.h"

#include "GLMSerializer.h"
#include "HuaEngine/Scene/SceneSerializer.h"
#include "HuaEngine/Rendering/Material/MaterialSerializer.h"

namespace HE::Serialization {

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

    // Serialize any object to YAML string
    template<typename T>
    std::string ToYaml(const T& object) {
        return SerializationManager::Instance().SerializeToString(object, SerializationFormat::YAML);
    }

    // Deserialize any object from YAML string
    template<typename T>
    bool FromYaml(const std::string& yamlString, T& object) {
        return SerializationManager::Instance().DeserializeFromString(yamlString, object, SerializationFormat::YAML);
    }

    // Serialize any object to YAML file
    template<typename T>
    bool SaveAsYaml(const T& object, const std::string& filename) {
        return SerializationManager::Instance().SerializeToFile(object, filename, SerializationFormat::YAML);
    }

    // Deserialize any object from YAML file
    template<typename T>
    bool LoadFromYaml(const std::string& filename, T& object) {
        return SerializationManager::Instance().DeserializeFromFile(filename, object, SerializationFormat::YAML);
    }

} // namespace HE::Serialization
