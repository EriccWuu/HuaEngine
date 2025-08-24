#include "enginepch.h"
#include "Serialization.h"
#include "SerializationManager.h"
#include "JsonSerializationBackend.h"

namespace HE::Serialization {

    // Initialize the serialization system
    void InitializeSerialization() {
        // Register JSON backend
        SerializationManager::Instance().RegisterBackend(
            SerializationFormat::JSON,
            []() -> std::unique_ptr<SerializationBackend> {
                return std::make_unique<JsonSerializationBackend>();
            }
        );

        // TODO: Register YAML backend when implemented
        // SerializationManager::Instance().RegisterBackend(
        //     SerializationFormat::YAML,
        //     []() -> std::unique_ptr<SerializationBackend> {
        //         return std::make_unique<YamlSerializationBackend>();
        //     }
        // );

        // TODO: Register Binary backend when implemented
        // SerializationManager::Instance().RegisterBackend(
        //     SerializationFormat::Binary,
        //     []() -> std::unique_ptr<SerializationBackend> {
        //         return std::make_unique<BinarySerializationBackend>();
        //     }
        // );
    }

} // namespace HE::Serialization