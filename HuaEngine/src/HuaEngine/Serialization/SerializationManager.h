#pragma once

#include "Serialization.h"
#include <unordered_map>
#include <memory>
#include <functional>

namespace HE {

    enum class SerializationFormat {
        JSON,
        YAML,
        Binary
    };

    class SerializationManager {
    public:
        static SerializationManager& Instance() {
            static SerializationManager instance;
            return instance;
        }

        // Register a backend for a specific format
        void RegisterBackend(SerializationFormat format, 
                           std::function<std::unique_ptr<SerializationBackend>()> factory);

        // Create a backend instance
        std::unique_ptr<SerializationBackend> CreateBackend(SerializationFormat format);

        // Convenience methods for common operations
        template<typename T>
        std::string SerializeToString(const T& object, SerializationFormat format) {
            auto backend = CreateBackend(format);
            if (!backend) return "";

            backend->Reset();
            SerializeValue(*backend, "", object);
            return backend->SaveToString();
        }

        template<typename T>
        bool DeserializeFromString(const std::string& data, T& object, SerializationFormat format) {
            auto backend = CreateBackend(format);
            if (!backend) return false;

            try {
                backend->LoadFromString(data);
                return DeserializeValue(*backend, "", object);
            } catch (...) {
                return false;
            }
        }

        template<typename T>
        bool SerializeToFile(const T& object, const std::string& filename, SerializationFormat format) {
            auto backend = CreateBackend(format);
            if (!backend) return false;

            try {
                backend->Reset();
                SerializeValue(*backend, "", object);
                backend->SaveToFile(filename);
                return true;
            } catch (...) {
                return false;
            }
        }

        template<typename T>
        bool DeserializeFromFile(const std::string& filename, T& object, SerializationFormat format) {
            auto backend = CreateBackend(format);
            if (!backend) return false;

            try {
                backend->LoadFromFile(filename);
                return DeserializeValue(*backend, "", object);
            } catch (...) {
                return false;
            }
        }

    private:
        SerializationManager() = default;
        std::unordered_map<SerializationFormat, std::function<std::unique_ptr<SerializationBackend>()>> m_Backends;
    };

    // Helper macros for easy serialization
    #define SERIALIZE_TO_STRING(object, format) \
        SerializationManager::Instance().SerializeToString(object, format)

    #define DESERIALIZE_FROM_STRING(data, object, format) \
        SerializationManager::Instance().DeserializeFromString(data, object, format)

    #define SERIALIZE_TO_FILE(object, filename, format) \
        SerializationManager::Instance().SerializeToFile(object, filename, format)

    #define DESERIALIZE_FROM_FILE(filename, object, format) \
        SerializationManager::Instance().DeserializeFromFile(filename, object, format)
}
