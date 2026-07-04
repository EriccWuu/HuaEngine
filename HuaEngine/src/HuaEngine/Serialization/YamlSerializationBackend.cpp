#include "enginepch.h"
#include "YamlSerializationBackend.h"

#include <fstream>
#include <sstream>

namespace HE::Serialization {

    YamlSerializationBackend::YamlSerializationBackend() {
        Reset();
    }

    void YamlSerializationBackend::Reset() {
        m_Root = YAML::Node(YAML::NodeType::Map);
        m_NodeStack.clear();
        while (!m_ArrayIndices.empty()) {
            m_ArrayIndices.pop();
        }

        m_NodeStack.push_back(m_Root);
        m_IsReading = false;
        m_StringStorage.clear();
    }

    YAML::Node YamlSerializationBackend::GetCurrentNode() const {
        return m_NodeStack.empty() ? YAML::Node() : m_NodeStack.back();
    }

    YAML::Node YamlSerializationBackend::GetNamedNode(const YAML::Node& current, const std::string& name) const {
        if (!current) {
            return YAML::Node();
        }

        if (current.IsSequence() && !m_ArrayIndices.empty()) {
            const size_t index = m_ArrayIndices.top();
            return index < current.size() ? current[index] : YAML::Node();
        }

        if (name.empty()) {
            return current;
        }

        if (current.IsMap()) {
            return current[name];
        }

        return YAML::Node();
    }

    YAML::Node YamlSerializationBackend::GetWritableArrayElement(YAML::Node& current) {
        if (!current.IsSequence() || m_ArrayIndices.empty()) {
            return YAML::Node();
        }

        const size_t index = m_ArrayIndices.top();
        while (current.size() <= index) {
            current.push_back(YAML::Node());
        }
        return current[index];
    }

    void YamlSerializationBackend::PushNode(const YAML::Node& node) {
        m_NodeStack.push_back(node ? node : YAML::Node(YAML::NodeType::Map));
    }

    template<typename T>
    void YamlSerializationBackend::SetValue(const std::string& name, const T& value) {
        YAML::Node current = GetCurrentNode();
        if (!current) {
            return;
        }

        if (current.IsMap() && !name.empty()) {
            current[name] = value;
            return;
        }

        if (current.IsSequence()) {
            YAML::Node element = GetWritableArrayElement(current);
            if (element) {
                element = value;
            }
            return;
        }

        if (name.empty()) {
            current = value;
        }
    }

    template<typename T>
    bool YamlSerializationBackend::GetValue(const std::string& name, T& value) const {
        YAML::Node current = GetCurrentNode();
        YAML::Node target = GetNamedNode(current, name);
        if (!target || !target.IsScalar()) {
            return false;
        }

        try {
            value = target.as<T>();
            return true;
        } catch (const YAML::Exception&) {
            return false;
        }
    }

    void YamlSerializationBackend::BeginObject(const std::string& name) {
        YAML::Node current = GetCurrentNode();
        if (!current) {
            return;
        }

        if (!m_IsReading) {
            if (current.IsMap() && !name.empty()) {
                current[name] = YAML::Node(YAML::NodeType::Map);
                PushNode(current[name]);
                return;
            }

            if (current.IsSequence()) {
                YAML::Node element = GetWritableArrayElement(current);
                element = YAML::Node(YAML::NodeType::Map);
                PushNode(element);
                return;
            }

            if (name.empty()) {
                PushNode(current);
                return;
            }
        } else {
            YAML::Node target = GetNamedNode(current, name);
            PushNode(target && target.IsMap() ? target : YAML::Node(YAML::NodeType::Map));
            return;
        }

        PushNode(YAML::Node(YAML::NodeType::Map));
    }

    void YamlSerializationBackend::EndObject() {
        if (!m_NodeStack.empty()) {
            m_NodeStack.pop_back();
        }
    }

    void YamlSerializationBackend::BeginArray(const std::string& name, size_t) {
        YAML::Node current = GetCurrentNode();
        if (!current) {
            return;
        }

        if (!m_IsReading) {
            if (current.IsMap()) {
                current[name] = YAML::Node(YAML::NodeType::Sequence);
                PushNode(current[name]);
                return;
            }

            if (current.IsSequence()) {
                YAML::Node element = GetWritableArrayElement(current);
                element = YAML::Node(YAML::NodeType::Sequence);
                PushNode(element);
                return;
            }
        } else {
            YAML::Node target = GetNamedNode(current, name);
            PushNode(target && target.IsSequence() ? target : YAML::Node(YAML::NodeType::Sequence));
            return;
        }

        PushNode(YAML::Node(YAML::NodeType::Sequence));
    }

    void YamlSerializationBackend::EndArray() {
        if (!m_NodeStack.empty()) {
            m_NodeStack.pop_back();
        }
    }

    void YamlSerializationBackend::BeginArrayElement(size_t index) {
        m_ArrayIndices.push(index);
    }

    void YamlSerializationBackend::EndArrayElement() {
        if (!m_ArrayIndices.empty()) {
            m_ArrayIndices.pop();
        }
    }

    void YamlSerializationBackend::Serialize(const std::string& name, bool value) { SetValue(name, value); }
    void YamlSerializationBackend::Serialize(const std::string& name, int8_t value) { SetValue(name, static_cast<int32_t>(value)); }
    void YamlSerializationBackend::Serialize(const std::string& name, int16_t value) { SetValue(name, static_cast<int32_t>(value)); }
    void YamlSerializationBackend::Serialize(const std::string& name, int32_t value) { SetValue(name, value); }
    void YamlSerializationBackend::Serialize(const std::string& name, int64_t value) { SetValue(name, value); }
    void YamlSerializationBackend::Serialize(const std::string& name, uint8_t value) { SetValue(name, static_cast<uint32_t>(value)); }
    void YamlSerializationBackend::Serialize(const std::string& name, uint16_t value) { SetValue(name, static_cast<uint32_t>(value)); }
    void YamlSerializationBackend::Serialize(const std::string& name, uint32_t value) { SetValue(name, value); }
    void YamlSerializationBackend::Serialize(const std::string& name, uint64_t value) { SetValue(name, value); }
    void YamlSerializationBackend::Serialize(const std::string& name, float value) { SetValue(name, value); }
    void YamlSerializationBackend::Serialize(const std::string& name, double value) { SetValue(name, value); }
    void YamlSerializationBackend::Serialize(const std::string& name, const std::string& value) { SetValue(name, value); }
    void YamlSerializationBackend::Serialize(const std::string& name, const char* value) { SetValue(name, std::string(value ? value : "")); }

    bool YamlSerializationBackend::Deserialize(const std::string& name, bool& value) { return GetValue(name, value); }
    bool YamlSerializationBackend::Deserialize(const std::string& name, int8_t& value) {
        int32_t temp = 0;
        if (!GetValue(name, temp)) return false;
        value = static_cast<int8_t>(temp);
        return true;
    }
    bool YamlSerializationBackend::Deserialize(const std::string& name, int16_t& value) {
        int32_t temp = 0;
        if (!GetValue(name, temp)) return false;
        value = static_cast<int16_t>(temp);
        return true;
    }
    bool YamlSerializationBackend::Deserialize(const std::string& name, int32_t& value) { return GetValue(name, value); }
    bool YamlSerializationBackend::Deserialize(const std::string& name, int64_t& value) { return GetValue(name, value); }
    bool YamlSerializationBackend::Deserialize(const std::string& name, uint8_t& value) {
        uint32_t temp = 0;
        if (!GetValue(name, temp)) return false;
        value = static_cast<uint8_t>(temp);
        return true;
    }
    bool YamlSerializationBackend::Deserialize(const std::string& name, uint16_t& value) {
        uint32_t temp = 0;
        if (!GetValue(name, temp)) return false;
        value = static_cast<uint16_t>(temp);
        return true;
    }
    bool YamlSerializationBackend::Deserialize(const std::string& name, uint32_t& value) { return GetValue(name, value); }
    bool YamlSerializationBackend::Deserialize(const std::string& name, uint64_t& value) { return GetValue(name, value); }
    bool YamlSerializationBackend::Deserialize(const std::string& name, float& value) { return GetValue(name, value); }
    bool YamlSerializationBackend::Deserialize(const std::string& name, double& value) { return GetValue(name, value); }
    bool YamlSerializationBackend::Deserialize(const std::string& name, std::string& value) { return GetValue(name, value); }
    bool YamlSerializationBackend::Deserialize(const std::string& name, const char*& value) {
        if (!GetValue(name, m_StringStorage)) {
            return false;
        }
        value = m_StringStorage.c_str();
        return true;
    }

    bool YamlSerializationBackend::HasField(const std::string& name) const {
        YAML::Node current = GetCurrentNode();
        if (!current || !current.IsMap()) {
            return false;
        }
        return static_cast<bool>(current[name]);
    }

    size_t YamlSerializationBackend::GetArraySize(const std::string& name) const {
        YAML::Node current = GetCurrentNode();
        YAML::Node target = GetNamedNode(current, name);
        return target && target.IsSequence() ? target.size() : 0;
    }

    SerializationType YamlSerializationBackend::GetFieldType(const std::string& name) const {
        YAML::Node current = GetCurrentNode();
        YAML::Node target = GetNamedNode(current, name);
        if (!target) {
            return SerializationType::Object;
        }

        if (target.IsSequence()) {
            return SerializationType::Array;
        }

        if (target.IsMap()) {
            return SerializationType::Object;
        }

        return SerializationType::String;
    }

    std::vector<std::string> YamlSerializationBackend::GetObjectKeys() const {
        std::vector<std::string> keys;
        YAML::Node current = GetCurrentNode();
        if (!current || !current.IsMap()) {
            return keys;
        }

        for (const auto& entry : current) {
            if (entry.first.IsScalar()) {
                keys.push_back(entry.first.as<std::string>());
            }
        }
        return keys;
    }

    void YamlSerializationBackend::ForEachField(const std::function<void(const std::string& key)>& callback) {
        YAML::Node current = GetCurrentNode();
        if (!current || !current.IsMap()) {
            return;
        }

        for (const auto& entry : current) {
            if (!entry.first.IsScalar()) {
                continue;
            }

            m_NodeStack.push_back(entry.second);
            callback(entry.first.as<std::string>());
            m_NodeStack.pop_back();
        }
    }

    void YamlSerializationBackend::LoadFromString(const std::string& data) {
        m_Root = data.empty() ? YAML::Node(YAML::NodeType::Map) : YAML::Load(data);
        if (!m_Root) {
            m_Root = YAML::Node(YAML::NodeType::Map);
        }

        m_NodeStack.clear();
        while (!m_ArrayIndices.empty()) {
            m_ArrayIndices.pop();
        }
        m_NodeStack.push_back(m_Root);
        m_IsReading = true;
    }

    void YamlSerializationBackend::LoadFromFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file: " + filename);
        }

        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        LoadFromString(content);
    }

    std::string YamlSerializationBackend::SaveToString() const {
        YAML::Emitter emitter;
        emitter << m_Root;
        return std::string(emitter.c_str());
    }

    void YamlSerializationBackend::SaveToFile(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to create file: " + filename);
        }

        file << SaveToString();
    }

} // namespace HE::Serialization
