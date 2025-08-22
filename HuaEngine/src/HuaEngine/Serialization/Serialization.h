#pragma once

#include <string>
#include <vector>
#include <memory>
#include <type_traits>
#include <functional>
#include <cstdint>
#include "HuaEngine/Core/Core.h"

namespace HE {

    // Forward declarations
    class SerializationBackend;
    class SerializationManager;

    // Serialization value types
    enum class SerializationType {
        Bool,
        Int8, Int16, Int32, Int64,
        UInt8, UInt16, UInt32, UInt64,
        Float, Double,
        String,
        Object,
        Array
    };

    // Base serialization interface
    class SerializationBackend {
    public:
        virtual ~SerializationBackend() = default;

        // Object operations
        virtual void BeginObject(const std::string& name = "") = 0;
        virtual void EndObject() = 0;

        // Array operations
        virtual void BeginArray(const std::string& name, size_t size = 0) = 0;
        virtual void EndArray() = 0;
        virtual void BeginArrayElement(size_t index) = 0;
        virtual void EndArrayElement() = 0;

        // Basic type serialization
        virtual void Serialize(const std::string& name, bool value) = 0;
        virtual void Serialize(const std::string& name, int8_t value) = 0;
        virtual void Serialize(const std::string& name, int16_t value) = 0;
        virtual void Serialize(const std::string& name, int32_t value) = 0;
        virtual void Serialize(const std::string& name, int64_t value) = 0;
        virtual void Serialize(const std::string& name, uint8_t value) = 0;
        virtual void Serialize(const std::string& name, uint16_t value) = 0;
        virtual void Serialize(const std::string& name, uint32_t value) = 0;
        virtual void Serialize(const std::string& name, uint64_t value) = 0;
        virtual void Serialize(const std::string& name, float value) = 0;
        virtual void Serialize(const std::string& name, double value) = 0;
        virtual void Serialize(const std::string& name, const std::string& value) = 0;
        virtual void Serialize(const std::string& name, const char* value) = 0;

        // Basic type deserialization
        virtual bool Deserialize(const std::string& name, bool& value) = 0;
        virtual bool Deserialize(const std::string& name, int8_t& value) = 0;
        virtual bool Deserialize(const std::string& name, int16_t& value) = 0;
        virtual bool Deserialize(const std::string& name, int32_t& value) = 0;
        virtual bool Deserialize(const std::string& name, int64_t& value) = 0;
        virtual bool Deserialize(const std::string& name, uint8_t& value) = 0;
        virtual bool Deserialize(const std::string& name, uint16_t& value) = 0;
        virtual bool Deserialize(const std::string& name, uint32_t& value) = 0;
        virtual bool Deserialize(const std::string& name, uint64_t& value) = 0;
        virtual bool Deserialize(const std::string& name, float& value) = 0;
        virtual bool Deserialize(const std::string& name, double& value) = 0;
        virtual bool Deserialize(const std::string& name, std::string& value) = 0;
        virtual bool Deserialize(const std::string& name, const char*& value) = 0;

        // Query operations
        virtual bool HasField(const std::string& name) const = 0;
        virtual size_t GetArraySize(const std::string& name) const = 0;
        virtual SerializationType GetFieldType(const std::string& name) const = 0;

        // IO operations
        virtual void LoadFromString(const std::string& data) = 0;
        virtual void LoadFromFile(const std::string& filename) = 0;
        virtual std::string SaveToString() const = 0;
        virtual void SaveToFile(const std::string& filename) const = 0;

        // Context management
        virtual void Reset() = 0;
        virtual bool IsReading() const = 0;
        virtual bool IsWriting() const = 0;

    protected:
        bool m_IsReading = false;
    };

    // Type traits for serialization
    template<typename T>
    struct is_serializable {
        static constexpr bool value = std::is_arithmetic_v<T> || 
                                    std::is_same_v<T, std::string> ||
                                    std::is_same_v<T, bool>;
    };

    template<typename T>
    constexpr bool is_serializable_v = is_serializable<T>::value;

    // Forward declaration for Serializer
    template<typename T>
    struct Serializer;

    // Generic serialization functions
    template<typename T>
    void SerializeValue(SerializationBackend& backend, const std::string& name, const T& value) {
        if constexpr (std::is_same_v<T, bool>) {
            backend.Serialize(name, value);
        } else if constexpr (std::is_same_v<T, int8_t>) {
            backend.Serialize(name, value);
        } else if constexpr (std::is_same_v<T, int16_t>) {
            backend.Serialize(name, value);
        } else if constexpr (std::is_same_v<T, int32_t>) {
            backend.Serialize(name, value);
        } else if constexpr (std::is_same_v<T, int64_t>) {
            backend.Serialize(name, value);
        } else if constexpr (std::is_same_v<T, uint8_t>) {
            backend.Serialize(name, value);
        } else if constexpr (std::is_same_v<T, uint16_t>) {
            backend.Serialize(name, value);
        } else if constexpr (std::is_same_v<T, uint32_t>) {
            backend.Serialize(name, value);
        } else if constexpr (std::is_same_v<T, uint64_t>) {
            backend.Serialize(name, value);
        } else if constexpr (std::is_same_v<T, float>) {
            backend.Serialize(name, value);
        } else if constexpr (std::is_same_v<T, double>) {
            backend.Serialize(name, value);
        } else if constexpr (std::is_same_v<T, std::string>) {
            backend.Serialize(name, value);
        } else if constexpr (std::is_same_v<T, int>) {
            backend.Serialize(name, static_cast<int32_t>(value));
        } else if constexpr (std::is_same_v<T, unsigned int>) {
            backend.Serialize(name, static_cast<uint32_t>(value));
        } else {
            // Use custom serializer for complex types
            Serializer<T>::Serialize(backend, name, value);
        }
    }

    template<typename T>
    bool DeserializeValue(SerializationBackend& backend, const std::string& name, T& value) {
        if constexpr (std::is_same_v<T, bool>) {
            return backend.Deserialize(name, value);
        } else if constexpr (std::is_same_v<T, int8_t>) {
            return backend.Deserialize(name, value);
        } else if constexpr (std::is_same_v<T, int16_t>) {
            return backend.Deserialize(name, value);
        } else if constexpr (std::is_same_v<T, int32_t>) {
            return backend.Deserialize(name, value);
        } else if constexpr (std::is_same_v<T, int64_t>) {
            return backend.Deserialize(name, value);
        } else if constexpr (std::is_same_v<T, uint8_t>) {
            return backend.Deserialize(name, value);
        } else if constexpr (std::is_same_v<T, uint16_t>) {
            return backend.Deserialize(name, value);
        } else if constexpr (std::is_same_v<T, uint32_t>) {
            return backend.Deserialize(name, value);
        } else if constexpr (std::is_same_v<T, uint64_t>) {
            return backend.Deserialize(name, value);
        } else if constexpr (std::is_same_v<T, float>) {
            return backend.Deserialize(name, value);
        } else if constexpr (std::is_same_v<T, double>) {
            return backend.Deserialize(name, value);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return backend.Deserialize(name, value);
        } else if constexpr (std::is_same_v<T, int>) {
            int32_t temp;
            bool result = backend.Deserialize(name, temp);
            if (result) value = static_cast<int>(temp);
            return result;
        } else if constexpr (std::is_same_v<T, unsigned int>) {
            uint32_t temp;
            bool result = backend.Deserialize(name, temp);
            if (result) value = static_cast<unsigned int>(temp);
            return result;
        } else {
            // Use custom serializer for complex types
            return Serializer<T>::Deserialize(backend, name, value);
        }
    }

    // Array serialization
    template<typename T>
    void SerializeArray(SerializationBackend& backend, const std::string& name, const std::vector<T>& array) {
        backend.BeginArray(name, array.size());
        for (size_t i = 0; i < array.size(); ++i) {
            backend.BeginArrayElement(i);
            SerializeValue(backend, "", array[i]);
            backend.EndArrayElement();
        }
        backend.EndArray();
    }

    template<typename T>
    bool DeserializeArray(SerializationBackend& backend, const std::string& name, std::vector<T>& array) {
        if (!backend.HasField(name)) {
            return false;
        }

        size_t size = backend.GetArraySize(name);
        array.resize(size);

        backend.BeginArray(name);
        for (size_t i = 0; i < size; ++i) {
            backend.BeginArrayElement(i);
            if (!DeserializeValue(backend, "", array[i])) {
                backend.EndArrayElement();
                backend.EndArray();
                return false;
            }
            backend.EndArrayElement();
        }
        backend.EndArray();
        return true;
    }
}