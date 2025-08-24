#pragma once

#include "SerializationCore.h"
#include "HuaEngine/Core/Core.h"
#include <unordered_map>
#include <memory>
#include <functional>

namespace HE::Serialization {

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

        // Ref<T> support methods
        template<typename T>
        std::string SerializeToString(const Ref<T>& object, SerializationFormat format) {
            if (!object) return "";
            
            auto backend = CreateBackend(format);
            if (!backend) return "";

            backend->Reset();
            SerializeValue(*backend, "", *object);
            return backend->SaveToString();
        }

        template<typename T>
        bool DeserializeFromString(const std::string& data, Ref<T>& object, SerializationFormat format) {
            auto backend = CreateBackend(format);
            if (!backend) return false;

            try {
                backend->LoadFromString(data);
                
                // Create object if it doesn't exist
                if (!object) {
                    object = CreateRef<T>();
                }
                
                return DeserializeValue(*backend, "", *object);
            } catch (...) {
                return false;
            }
        }

        template<typename T>
        bool SerializeToFile(const Ref<T>& object, const std::string& filename, SerializationFormat format) {
            if (!object) return false;
            
            auto backend = CreateBackend(format);
            if (!backend) return false;

            try {
                backend->Reset();
                SerializeValue(*backend, "", *object);
                backend->SaveToFile(filename);
                return true;
            } catch (...) {
                return false;
            }
        }

        template<typename T>
        bool DeserializeFromFile(const std::string& filename, Ref<T>& object, SerializationFormat format) {
            auto backend = CreateBackend(format);
            if (!backend) return false;

            try {
                backend->LoadFromFile(filename);
                
                // Create object if it doesn't exist
                if (!object) {
                    object = CreateRef<T>();
                }
                
                return DeserializeValue(*backend, "", *object);
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
        HE::Serialization::SerializationManager::Instance().SerializeToString(object, format)

    #define DESERIALIZE_FROM_STRING(data, object, format) \
        HE::Serialization::SerializationManager::Instance().DeserializeFromString(data, object, format)

    #define SERIALIZE_TO_FILE(object, filename, format) \
        HE::Serialization::SerializationManager::Instance().SerializeToFile(object, filename, format)

    #define DESERIALIZE_FROM_FILE(filename, object, format) \
        HE::Serialization::SerializationManager::Instance().DeserializeFromFile(filename, object, format)

    // Helper macros for Ref<T> serialization
    #define SERIALIZE_REF_TO_STRING(ref_object, format) \
        HE::Serialization::SerializationManager::Instance().SerializeToString(ref_object, format)

    #define DESERIALIZE_REF_FROM_STRING(data, ref_object, format) \
        HE::Serialization::SerializationManager::Instance().DeserializeFromString(data, ref_object, format)

    #define SERIALIZE_REF_TO_FILE(ref_object, filename, format) \
        HE::Serialization::SerializationManager::Instance().SerializeToFile(ref_object, filename, format)

    #define DESERIALIZE_REF_FROM_FILE(filename, ref_object, format) \
        HE::Serialization::SerializationManager::Instance().DeserializeFromFile(filename, ref_object, format)

} // namespace HE::Serialization
