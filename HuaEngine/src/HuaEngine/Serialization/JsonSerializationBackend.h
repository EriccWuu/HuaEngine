#pragma once

#include "SerializationCore.h"
#include <sstream>
#include <stack>
#include <variant>
#include <map>
#include <memory>

namespace HE::Serialization {

    // Simple JSON value type
    using JsonValue = std::variant<
        bool, int32_t, int64_t, uint32_t, uint64_t, 
        float, double, std::string
    >;

    // Simple JSON node structure
    struct JsonNode {
        enum Type { Object, Array, Value } type = Object;
        std::map<std::string, std::shared_ptr<JsonNode>> objectData;
        std::vector<std::shared_ptr<JsonNode>> arrayData;
        JsonValue valueData;

        JsonNode(Type t = Object) : type(t) {}
    };

    class JsonSerializationBackend : public SerializationBackend {
    public:
        JsonSerializationBackend();
        virtual ~JsonSerializationBackend() = default;

        // Object operations
        void BeginObject(const std::string& name = "") override;
        void EndObject() override;

        // Array operations
        void BeginArray(const std::string& name, size_t size = 0) override;
        void EndArray() override;
        void BeginArrayElement(size_t index) override;
        void EndArrayElement() override;

        // Basic type serialization
        void Serialize(const std::string& name, bool value) override;
        void Serialize(const std::string& name, int8_t value) override;
        void Serialize(const std::string& name, int16_t value) override;
        void Serialize(const std::string& name, int32_t value) override;
        void Serialize(const std::string& name, int64_t value) override;
        void Serialize(const std::string& name, uint8_t value) override;
        void Serialize(const std::string& name, uint16_t value) override;
        void Serialize(const std::string& name, uint32_t value) override;
        void Serialize(const std::string& name, uint64_t value) override;
        void Serialize(const std::string& name, float value) override;
        void Serialize(const std::string& name, double value) override;
        void Serialize(const std::string& name, const std::string& value) override;
        void Serialize(const std::string& name, const char* value) override;

        // Basic type deserialization
        bool Deserialize(const std::string& name, bool& value) override;
        bool Deserialize(const std::string& name, int8_t& value) override;
        bool Deserialize(const std::string& name, int16_t& value) override;
        bool Deserialize(const std::string& name, int32_t& value) override;
        bool Deserialize(const std::string& name, int64_t& value) override;
        bool Deserialize(const std::string& name, uint8_t& value) override;
        bool Deserialize(const std::string& name, uint16_t& value) override;
        bool Deserialize(const std::string& name, uint32_t& value) override;
        bool Deserialize(const std::string& name, uint64_t& value) override;
        bool Deserialize(const std::string& name, float& value) override;
        bool Deserialize(const std::string& name, double& value) override;
        bool Deserialize(const std::string& name, std::string& value) override;
        bool Deserialize(const std::string& name, const char*& value) override;

        // Query operations
        bool HasField(const std::string& name) const override;
        size_t GetArraySize(const std::string& name) const override;
        SerializationType GetFieldType(const std::string& name) const override;

        // Object key iteration
        std::vector<std::string> GetObjectKeys() const override;
        void ForEachField(const std::function<void(const std::string& key)>& callback) override;

        // Helper: Extract value from JsonValue variant
        template<typename T>
        static bool GetValueFromJson(const JsonValue& jsonValue, T& outValue) {
            if constexpr (std::is_same_v<T, bool>) {
                if (auto* ptr = std::get_if<bool>(&jsonValue)) { outValue = *ptr; return true; }
            } else if constexpr (std::is_same_v<T, int32_t>) {
                if (auto* ptr = std::get_if<int32_t>(&jsonValue)) { outValue = *ptr; return true; }
            } else if constexpr (std::is_same_v<T, int64_t>) {
                if (auto* ptr = std::get_if<int64_t>(&jsonValue)) { outValue = *ptr; return true; }
            } else if constexpr (std::is_same_v<T, uint32_t>) {
                if (auto* ptr = std::get_if<uint32_t>(&jsonValue)) { outValue = *ptr; return true; }
                // Also try int32_t for compatibility
                if (auto* ptr = std::get_if<int32_t>(&jsonValue)) {
                    if (*ptr >= 0) { outValue = static_cast<uint32_t>(*ptr); return true; }
                }
            } else if constexpr (std::is_same_v<T, uint64_t>) {
                if (auto* ptr = std::get_if<uint64_t>(&jsonValue)) { outValue = *ptr; return true; }
            } else if constexpr (std::is_same_v<T, float>) {
                if (auto* ptr = std::get_if<float>(&jsonValue)) { outValue = *ptr; return true; }
                if (auto* ptr = std::get_if<double>(&jsonValue)) { outValue = static_cast<float>(*ptr); return true; }
            } else if constexpr (std::is_same_v<T, double>) {
                if (auto* ptr = std::get_if<double>(&jsonValue)) { outValue = *ptr; return true; }
                if (auto* ptr = std::get_if<float>(&jsonValue)) { outValue = static_cast<double>(*ptr); return true; }
            } else if constexpr (std::is_same_v<T, std::string>) {
                if (auto* ptr = std::get_if<std::string>(&jsonValue)) { outValue = *ptr; return true; }
            }
            return false;
        }

        // IO operations
        void LoadFromString(const std::string& data) override;
        void LoadFromFile(const std::string& filename) override;
        std::string SaveToString() const override;
        void SaveToFile(const std::string& filename) const override;

        // Context management
        void Reset() override;
        bool IsReading() const override { return m_IsReading; }
        bool IsWriting() const override { return !m_IsReading; }

    private:
        std::shared_ptr<JsonNode> m_Root;
        std::stack<std::shared_ptr<JsonNode>> m_NodeStack;
        std::stack<size_t> m_ArrayIndices;

        std::shared_ptr<JsonNode> GetCurrentNode();
        void SetValue(const std::string& name, const JsonValue& value);
        bool GetValue(const std::string& name, JsonValue& value) const;

        // JSON parsing and serialization helpers
        std::string NodeToString(const std::shared_ptr<JsonNode>& node, int indent = 0) const;
        std::shared_ptr<JsonNode> ParseJson(const std::string& json);
        
        // Simple JSON parser helpers
        std::string ParseString(const std::string& json, size_t& pos) const;
        JsonValue ParseValue(const std::string& json, size_t& pos) const;
        std::shared_ptr<JsonNode> ParseObject(const std::string& json, size_t& pos) const;
        std::shared_ptr<JsonNode> ParseArray(const std::string& json, size_t& pos) const;
        void SkipWhitespace(const std::string& json, size_t& pos) const;
        
        // String escape helpers
        std::string EscapeString(const std::string& str) const;
        std::string UnescapeString(const std::string& str) const;
    };

} // namespace HE::Serialization
