#pragma once

#include "SerializationCore.h"

#include <stack>
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

namespace HE::Serialization {

    class YamlSerializationBackend : public SerializationBackend {
    public:
        YamlSerializationBackend();
        ~YamlSerializationBackend() override = default;

        void BeginObject(const std::string& name = "") override;
        void EndObject() override;

        void BeginArray(const std::string& name, size_t size = 0) override;
        void EndArray() override;
        void BeginArrayElement(size_t index) override;
        void EndArrayElement() override;

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

        bool HasField(const std::string& name) const override;
        size_t GetArraySize(const std::string& name) const override;
        SerializationType GetFieldType(const std::string& name) const override;

        std::vector<std::string> GetObjectKeys() const override;
        void ForEachField(const std::function<void(const std::string& key)>& callback) override;

        void LoadFromString(const std::string& data) override;
        void LoadFromFile(const std::string& filename) override;
        std::string SaveToString() const override;
        void SaveToFile(const std::string& filename) const override;

        void Reset() override;
        bool IsReading() const override { return m_IsReading; }
        bool IsWriting() const override { return !m_IsReading; }

    private:
        YAML::Node m_Root;
        std::vector<YAML::Node> m_NodeStack;
        std::stack<size_t> m_ArrayIndices;
        std::string m_StringStorage;

        YAML::Node GetCurrentNode() const;
        YAML::Node GetNamedNode(const YAML::Node& current, const std::string& name) const;
        YAML::Node GetWritableArrayElement(YAML::Node& current);
        void PushNode(const YAML::Node& node);

        template<typename T>
        void SetValue(const std::string& name, const T& value);

        template<typename T>
        bool GetValue(const std::string& name, T& value) const;
    };

} // namespace HE::Serialization
