#pragma once

#include "Serialization.h"
#include <yaml-cpp/yaml.h>
#include <stack>

namespace HE {

    class YamlSerializationBackend : public SerializationBackend {
    public:
        YamlSerializationBackend();
        virtual ~YamlSerializationBackend() = default;

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
        YAML::Node m_RootNode;
        std::stack<YAML::Node> m_NodeStack;
        std::stack<std::string> m_CurrentArrayName;
        std::stack<size_t> m_ArrayIndices;

        YAML::Node GetCurrentNode();
        void SetValue(const std::string& name, const YAML::Node& value);
        bool GetValue(const std::string& name, YAML::Node& value) const;
    };

}
