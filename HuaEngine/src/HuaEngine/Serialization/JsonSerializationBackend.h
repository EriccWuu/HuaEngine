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
