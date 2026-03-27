#include "enginepch.h"
#include "JsonSerializationBackend.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace HE::Serialization {

    JsonSerializationBackend::JsonSerializationBackend() {
        Reset();
    }

    void JsonSerializationBackend::Reset() {
        m_Root = std::make_shared<JsonNode>(JsonNode::Object);
        while (!m_NodeStack.empty()) m_NodeStack.pop();
        while (!m_ArrayIndices.empty()) m_ArrayIndices.pop();
        
        m_NodeStack.push(m_Root);
        m_IsReading = false;
    }

    std::shared_ptr<JsonNode> JsonSerializationBackend::GetCurrentNode() {
        return m_NodeStack.empty() ? nullptr : m_NodeStack.top();
    }

    void JsonSerializationBackend::SetValue(const std::string& name, const JsonValue& value) {
        auto current = GetCurrentNode();
        if (!current) return;

        auto valueNode = std::make_shared<JsonNode>(JsonNode::Value);
        valueNode->valueData = value;

        if (current->type == JsonNode::Object) {
            current->objectData[name] = valueNode;
        } else if (current->type == JsonNode::Array && !m_ArrayIndices.empty()) {
            size_t index = m_ArrayIndices.top();
            while (current->arrayData.size() <= index) {
                current->arrayData.push_back(nullptr);
            }
            current->arrayData[index] = valueNode;
        }
    }

    bool JsonSerializationBackend::GetValue(const std::string& name, JsonValue& value) const {
        auto current = m_NodeStack.empty() ? nullptr : m_NodeStack.top();
        if (!current) return false;

        std::shared_ptr<JsonNode> targetNode = nullptr;

        if (current->type == JsonNode::Object) {
            auto it = current->objectData.find(name);
            if (it != current->objectData.end()) {
                targetNode = it->second;
            }
        } else if (current->type == JsonNode::Array && !m_ArrayIndices.empty()) {
            size_t index = m_ArrayIndices.top();
            if (index < current->arrayData.size()) {
                targetNode = current->arrayData[index];
            }
        }

        if (targetNode && targetNode->type == JsonNode::Value) {
            value = targetNode->valueData;
            return true;
        }

        return false;
    }

    // Object operations
    void JsonSerializationBackend::BeginObject(const std::string& name) {
        auto current = GetCurrentNode();
        if (!current) return;

        auto objNode = std::make_shared<JsonNode>(JsonNode::Object);

        if (!m_IsReading) {
            if (current->type == JsonNode::Object && !name.empty()) {
                current->objectData[name] = objNode;
            } else if (current->type == JsonNode::Array && !m_ArrayIndices.empty()) {
                size_t index = m_ArrayIndices.top();
                while (current->arrayData.size() <= index) {
                    current->arrayData.push_back(nullptr);
                }
                current->arrayData[index] = objNode;
            } else if (name.empty() && current == m_Root) {
                // Root object case
                objNode = current;
            }
        } else {
            // Reading mode - navigate to existing object
            if (current->type == JsonNode::Object && !name.empty()) {
                auto it = current->objectData.find(name);
                if (it != current->objectData.end()) {
                    objNode = it->second;
                }
            } else if (name.empty() && current == m_Root) {
                objNode = current;
            } else if (current->type == JsonNode::Array && !m_ArrayIndices.empty()) {
                size_t index = m_ArrayIndices.top();
                if (index < current->arrayData.size()) {
                    objNode = current->arrayData[index];
                }
            }
        }

        m_NodeStack.push(objNode);
    }

    void JsonSerializationBackend::EndObject() {
        if (!m_NodeStack.empty()) {
            m_NodeStack.pop();
        }
    }

    // Array operations
    void JsonSerializationBackend::BeginArray(const std::string& name, size_t size) {
        auto current = GetCurrentNode();
        if (!current) return;

        auto arrayNode = std::make_shared<JsonNode>(JsonNode::Array);

        if (!m_IsReading) {
            if (current->type == JsonNode::Object) {
                current->objectData[name] = arrayNode;
            }
        } else {
            // Reading mode - navigate to existing array
            if (current->type == JsonNode::Object) {
                auto it = current->objectData.find(name);
                if (it != current->objectData.end()) {
                    arrayNode = it->second;
                }
            }
        }

        m_NodeStack.push(arrayNode);
    }

    void JsonSerializationBackend::EndArray() {
        if (!m_NodeStack.empty()) {
            m_NodeStack.pop();
        }
    }

    void JsonSerializationBackend::BeginArrayElement(size_t index) {
        m_ArrayIndices.push(index);
        
        // Don't create any node here - let the serializer decide what type of node to create
        // The node will be created when SerializeValue is called
    }

    void JsonSerializationBackend::EndArrayElement() {
        if (!m_ArrayIndices.empty()) {
            m_ArrayIndices.pop();
        }
    }

    // Basic type serialization
    void JsonSerializationBackend::Serialize(const std::string& name, bool value) {
        SetValue(name, value);
    }

    void JsonSerializationBackend::Serialize(const std::string& name, int8_t value) {
        SetValue(name, static_cast<int32_t>(value));
    }

    void JsonSerializationBackend::Serialize(const std::string& name, int16_t value) {
        SetValue(name, static_cast<int32_t>(value));
    }

    void JsonSerializationBackend::Serialize(const std::string& name, int32_t value) {
        SetValue(name, value);
    }

    void JsonSerializationBackend::Serialize(const std::string& name, int64_t value) {
        SetValue(name, value);
    }

    void JsonSerializationBackend::Serialize(const std::string& name, uint8_t value) {
        SetValue(name, static_cast<uint32_t>(value));
    }

    void JsonSerializationBackend::Serialize(const std::string& name, uint16_t value) {
        SetValue(name, static_cast<uint32_t>(value));
    }

    void JsonSerializationBackend::Serialize(const std::string& name, uint32_t value) {
        SetValue(name, value);
    }

    void JsonSerializationBackend::Serialize(const std::string& name, uint64_t value) {
        SetValue(name, value);
    }

    void JsonSerializationBackend::Serialize(const std::string& name, float value) {
        SetValue(name, value);
    }

    void JsonSerializationBackend::Serialize(const std::string& name, double value) {
        SetValue(name, value);
    }

    void JsonSerializationBackend::Serialize(const std::string& name, const std::string& value) {
        SetValue(name, value);
    }
    
    void JsonSerializationBackend::Serialize(const std::string& name, const char* value) {
        SetValue(name, std::string(value ? value : ""));
    }

    // Basic type deserialization
    bool JsonSerializationBackend::Deserialize(const std::string& name, bool& value) {
        JsonValue jsonValue;
        if (!GetValue(name, jsonValue)) return false;
        if (auto* boolPtr = std::get_if<bool>(&jsonValue)) {
            value = *boolPtr;
            return true;
        }
        return false;
    }

    bool JsonSerializationBackend::Deserialize(const std::string& name, int8_t& value) {
        JsonValue jsonValue;
        if (!GetValue(name, jsonValue)) return false;
        if (auto* intPtr = std::get_if<int32_t>(&jsonValue)) {
            value = static_cast<int8_t>(*intPtr);
            return true;
        }
        return false;
    }

    bool JsonSerializationBackend::Deserialize(const std::string& name, int16_t& value) {
        JsonValue jsonValue;
        if (!GetValue(name, jsonValue)) return false;
        if (auto* intPtr = std::get_if<int32_t>(&jsonValue)) {
            value = static_cast<int16_t>(*intPtr);
            return true;
        }
        return false;
    }

    bool JsonSerializationBackend::Deserialize(const std::string& name, int32_t& value) {
        JsonValue jsonValue;
        if (!GetValue(name, jsonValue)) return false;
        if (auto* intPtr = std::get_if<int32_t>(&jsonValue)) {
            value = *intPtr;
            return true;
        }
        return false;
    }

    bool JsonSerializationBackend::Deserialize(const std::string& name, int64_t& value) {
        JsonValue jsonValue;
        if (!GetValue(name, jsonValue)) return false;
        if (auto* intPtr = std::get_if<int64_t>(&jsonValue)) {
            value = *intPtr;
            return true;
        }
        return false;
    }

    bool JsonSerializationBackend::Deserialize(const std::string& name, uint8_t& value) {
        JsonValue jsonValue;
        if (!GetValue(name, jsonValue)) return false;
        
        if (auto* uintPtr = std::get_if<uint32_t>(&jsonValue)) {
            if (*uintPtr <= UINT8_MAX) {
                value = static_cast<uint8_t>(*uintPtr);
                return true;
            }
        }
        // Try to convert from int32_t
        if (auto* intPtr = std::get_if<int32_t>(&jsonValue)) {
            if (*intPtr >= 0 && *intPtr <= UINT8_MAX) {
                value = static_cast<uint8_t>(*intPtr);
                return true;
            }
        }
        return false;
    }

    bool JsonSerializationBackend::Deserialize(const std::string& name, uint16_t& value) {
        JsonValue jsonValue;
        if (!GetValue(name, jsonValue)) return false;
        if (auto* uintPtr = std::get_if<uint32_t>(&jsonValue)) {
            value = static_cast<uint16_t>(*uintPtr);
            return true;
        }
        return false;
    }

    bool JsonSerializationBackend::Deserialize(const std::string& name, uint32_t& value) {
        JsonValue jsonValue;
        if (!GetValue(name, jsonValue)) return false;
        
        if (auto* uintPtr = std::get_if<uint32_t>(&jsonValue)) {
            value = *uintPtr;
            return true;
        }
        // Try to convert from int32_t
        if (auto* intPtr = std::get_if<int32_t>(&jsonValue)) {
            if (*intPtr >= 0) {
                value = static_cast<uint32_t>(*intPtr);
                return true;
            }
        }
        return false;
    }

    bool JsonSerializationBackend::Deserialize(const std::string& name, uint64_t& value) {
        JsonValue jsonValue;
        if (!GetValue(name, jsonValue)) return false;
        if (auto* uintPtr = std::get_if<uint64_t>(&jsonValue)) {
            value = *uintPtr;
            return true;
        }
        return false;
    }

    bool JsonSerializationBackend::Deserialize(const std::string& name, float& value) {
        JsonValue jsonValue;
        if (!GetValue(name, jsonValue)) return false;
        if (auto* floatPtr = std::get_if<float>(&jsonValue)) {
            value = *floatPtr;
            return true;
        }
        if (auto* doublePtr = std::get_if<double>(&jsonValue)) {
            value = static_cast<float>(*doublePtr);
            return true;
        }
        return false;
    }

    bool JsonSerializationBackend::Deserialize(const std::string& name, double& value) {
        JsonValue jsonValue;
        if (!GetValue(name, jsonValue)) return false;
        if (auto* doublePtr = std::get_if<double>(&jsonValue)) {
            value = *doublePtr;
            return true;
        }
        if (auto* floatPtr = std::get_if<float>(&jsonValue)) {
            value = static_cast<double>(*floatPtr);
            return true;
        }
        return false;
    }

    bool JsonSerializationBackend::Deserialize(const std::string& name, std::string& value) {
        JsonValue jsonValue;
        if (!GetValue(name, jsonValue)) return false;
        if (auto* strPtr = std::get_if<std::string>(&jsonValue)) {
            value = *strPtr;
            return true;
        }
        return false;
    }

    bool JsonSerializationBackend::Deserialize(const std::string& name, const char*& value) {
        // Note: This implementation requires careful memory management
        // For now, we'll use a static storage approach
        static std::string temp_storage;
        JsonValue jsonValue;
        if (!GetValue(name, jsonValue)) return false;
        if (auto* strPtr = std::get_if<std::string>(&jsonValue)) {
            temp_storage = *strPtr;
            value = temp_storage.c_str();
            return true;
        }
        return false;
    }

    // Query operations
    bool JsonSerializationBackend::HasField(const std::string& name) const {
        auto current = m_NodeStack.empty() ? nullptr : m_NodeStack.top();
        if (!current || current->type != JsonNode::Object) return false;
        return current->objectData.find(name) != current->objectData.end();
    }

    size_t JsonSerializationBackend::GetArraySize(const std::string& name) const {
        auto current = m_NodeStack.empty() ? nullptr : m_NodeStack.top();
        if (!current || current->type != JsonNode::Object) return 0;
        
        auto it = current->objectData.find(name);
        if (it != current->objectData.end() && it->second->type == JsonNode::Array) {
            return it->second->arrayData.size();
        }
        return 0;
    }

    SerializationType JsonSerializationBackend::GetFieldType(const std::string& name) const {
        auto current = m_NodeStack.empty() ? nullptr : m_NodeStack.top();
        if (!current || current->type != JsonNode::Object) return SerializationType::Object;

        auto it = current->objectData.find(name);
        if (it != current->objectData.end()) {
            auto& node = it->second;
            if (node->type == JsonNode::Array) return SerializationType::Array;
            if (node->type == JsonNode::Object) return SerializationType::Object;
            if (node->type == JsonNode::Value) {
                const auto& value = node->valueData;
                if (std::holds_alternative<bool>(value)) return SerializationType::Bool;
                if (std::holds_alternative<int32_t>(value)) return SerializationType::Int32;
                if (std::holds_alternative<uint32_t>(value)) return SerializationType::UInt32;
                if (std::holds_alternative<float>(value)) return SerializationType::Float;
                if (std::holds_alternative<double>(value)) return SerializationType::Double;
                if (std::holds_alternative<std::string>(value)) return SerializationType::String;
            }
        }
        return SerializationType::Object;
    }

    std::vector<std::string> JsonSerializationBackend::GetObjectKeys() const {
        std::vector<std::string> keys;
        auto current = m_NodeStack.empty() ? nullptr : m_NodeStack.top();

        if (current && current->type == JsonNode::Object) {
            for (const auto& [key, value] : current->objectData) {
                keys.push_back(key);
            }
        }

        return keys;
    }

    void JsonSerializationBackend::ForEachField(const std::function<void(const std::string& key)>& callback) {
        auto current = m_NodeStack.empty() ? nullptr : m_NodeStack.top();

        if (!current || current->type != JsonNode::Object) {
            return;
        }

        for (const auto& [key, valueNode] : current->objectData) {
            // Push the value node onto the stack so the callback can access it
            m_NodeStack.push(valueNode);
            callback(key);
            m_NodeStack.pop();
        }
    }

    // String helpers
    std::string JsonSerializationBackend::EscapeString(const std::string& str) const {
        std::string escaped;
        escaped.reserve(str.length() + 10);
        
        for (char c : str) {
            switch (c) {
                case '"': escaped += "\\\""; break;
                case '\\': escaped += "\\\\"; break;
                case '\b': escaped += "\\b"; break;
                case '\f': escaped += "\\f"; break;
                case '\n': escaped += "\\n"; break;
                case '\r': escaped += "\\r"; break;
                case '\t': escaped += "\\t"; break;
                default: escaped += c; break;
            }
        }
        
        return escaped;
    }

    // JSON to string conversion
    std::string JsonSerializationBackend::NodeToString(const std::shared_ptr<JsonNode>& node, int indent) const {
        if (!node) return "null";
        
        std::string indentStr(indent * 2, ' ');
        std::string nextIndentStr((indent + 1) * 2, ' ');
        
        switch (node->type) {
            case JsonNode::Value: {
                const auto& value = node->valueData;
                if (auto* boolPtr = std::get_if<bool>(&value)) {
                    return *boolPtr ? "true" : "false";
                } else if (auto* intPtr = std::get_if<int32_t>(&value)) {
                    return std::to_string(*intPtr);
                } else if (auto* int64Ptr = std::get_if<int64_t>(&value)) {
                    return std::to_string(*int64Ptr);
                } else if (auto* uintPtr = std::get_if<uint32_t>(&value)) {
                    return std::to_string(*uintPtr);
                } else if (auto* uint64Ptr = std::get_if<uint64_t>(&value)) {
                    return std::to_string(*uint64Ptr);
                } else if (auto* floatPtr = std::get_if<float>(&value)) {
                    return std::to_string(*floatPtr);
                } else if (auto* doublePtr = std::get_if<double>(&value)) {
                    return std::to_string(*doublePtr);
                } else if (auto* strPtr = std::get_if<std::string>(&value)) {
                    return "\"" + EscapeString(*strPtr) + "\"";
                }
                return "null";
            }
            
            case JsonNode::Array: {
                std::string result = "[\n";
                for (size_t i = 0; i < node->arrayData.size(); ++i) {
                    result += nextIndentStr + NodeToString(node->arrayData[i], indent + 1);
                    if (i < node->arrayData.size() - 1) result += ",";
                    result += "\n";
                }
                result += indentStr + "]";
                return result;
            }
            
            case JsonNode::Object: {
                std::string result = "{\n";
                bool first = true;
                for (const auto& [key, childNode] : node->objectData) {
                    if (!first) result += ",\n";
                    result += nextIndentStr + "\"" + EscapeString(key) + "\": " + 
                             NodeToString(childNode, indent + 1);
                    first = false;
                }
                if (!first) result += "\n";
                result += indentStr + "}";
                return result;
            }
        }
        
        return "null";
    }

    // IO operations
    void JsonSerializationBackend::LoadFromString(const std::string& data) {
        try {
            m_Root = ParseJson(data);
            m_IsReading = true;
            
            while (!m_NodeStack.empty()) m_NodeStack.pop();
            m_NodeStack.push(m_Root);
        } catch (const std::exception& e) {
            std::cerr << "Failed to parse JSON: " << e.what() << std::endl;
            throw;
        }
    }

    void JsonSerializationBackend::LoadFromFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file: " + filename);
        }

        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        LoadFromString(content);
    }

    std::string JsonSerializationBackend::SaveToString() const {
        return NodeToString(m_Root);
    }

    void JsonSerializationBackend::SaveToFile(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to create file: " + filename);
        }
        
        file << SaveToString();
    }

    // Simple JSON parser - basic implementation
    void JsonSerializationBackend::SkipWhitespace(const std::string& json, size_t& pos) const {
        while (pos < json.length() && std::isspace(json[pos])) {
            ++pos;
        }
    }

    std::shared_ptr<JsonNode> JsonSerializationBackend::ParseJson(const std::string& json) {
        size_t pos = 0;
        SkipWhitespace(json, pos);
        
        if (pos >= json.length()) {
            return std::make_shared<JsonNode>(JsonNode::Object);
        }
        
        if (json[pos] == '{') {
            return ParseObject(json, pos);
        } else if (json[pos] == '[') {
            return ParseArray(json, pos);
        } else {
            // Single value
            auto valueNode = std::make_shared<JsonNode>(JsonNode::Value);
            valueNode->valueData = ParseValue(json, pos);
            return valueNode;
        }
    }

    std::shared_ptr<JsonNode> JsonSerializationBackend::ParseObject(const std::string& json, size_t& pos) const {
        auto objNode = std::make_shared<JsonNode>(JsonNode::Object);
        
        if (pos >= json.length() || json[pos] != '{') {
            throw std::runtime_error("Expected '{'");
        }
        ++pos; // Skip '{'
        
        SkipWhitespace(json, pos);
        
        // Empty object
        if (pos < json.length() && json[pos] == '}') {
            ++pos;
            return objNode;
        }
        
        while (pos < json.length()) {
            SkipWhitespace(json, pos);
            
            // Parse key (must be string)
            if (pos >= json.length() || json[pos] != '"') {
                throw std::runtime_error("Expected string key");
            }
            std::string key = ParseString(json, pos);
            
            SkipWhitespace(json, pos);
            
            // Expect ':'
            if (pos >= json.length() || json[pos] != ':') {
                throw std::runtime_error("Expected ':'");
            }
            ++pos;
            
            SkipWhitespace(json, pos);
            
            // Parse value
            std::shared_ptr<JsonNode> valueNode;
            if (pos < json.length()) {
                if (json[pos] == '{') {
                    valueNode = ParseObject(json, pos);
                } else if (json[pos] == '[') {
                    valueNode = ParseArray(json, pos);
                } else {
                    valueNode = std::make_shared<JsonNode>(JsonNode::Value);
                    valueNode->valueData = ParseValue(json, pos);
                }
            }
            
            objNode->objectData[key] = valueNode;
            
            SkipWhitespace(json, pos);
            
            if (pos >= json.length()) break;
            
            if (json[pos] == '}') {
                ++pos;
                break;
            } else if (json[pos] == ',') {
                ++pos;
                continue;
            } else {
                throw std::runtime_error("Expected ',' or '}'");
            }
        }
        
        return objNode;
    }

    std::shared_ptr<JsonNode> JsonSerializationBackend::ParseArray(const std::string& json, size_t& pos) const {
        auto arrayNode = std::make_shared<JsonNode>(JsonNode::Array);
        
        if (pos >= json.length() || json[pos] != '[') {
            throw std::runtime_error("Expected '['");
        }
        ++pos; // Skip '['
        
        SkipWhitespace(json, pos);
        
        // Empty array
        if (pos < json.length() && json[pos] == ']') {
            ++pos;
            return arrayNode;
        }
        
        while (pos < json.length()) {
            SkipWhitespace(json, pos);
            
            // Parse element
            std::shared_ptr<JsonNode> elementNode;
            if (pos < json.length()) {
                if (json[pos] == '{') {
                    elementNode = ParseObject(json, pos);
                } else if (json[pos] == '[') {
                    elementNode = ParseArray(json, pos);
                } else {
                    elementNode = std::make_shared<JsonNode>(JsonNode::Value);
                    elementNode->valueData = ParseValue(json, pos);
                }
            }
            
            arrayNode->arrayData.push_back(elementNode);
            
            SkipWhitespace(json, pos);
            
            if (pos >= json.length()) break;
            
            if (json[pos] == ']') {
                ++pos;
                break;
            } else if (json[pos] == ',') {
                ++pos;
                continue;
            } else {
                throw std::runtime_error("Expected ',' or ']'");
            }
        }
        
        return arrayNode;
    }

    std::string JsonSerializationBackend::ParseString(const std::string& json, size_t& pos) const {
        if (pos >= json.length() || json[pos] != '"') {
            throw std::runtime_error("Expected '\"'");
        }
        ++pos; // Skip opening quote
        
        std::string result;
        while (pos < json.length() && json[pos] != '"') {
            if (json[pos] == '\\' && pos + 1 < json.length()) {
                ++pos;
                switch (json[pos]) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    default: result += json[pos]; break;
                }
            } else {
                result += json[pos];
            }
            ++pos;
        }
        
        if (pos >= json.length()) {
            throw std::runtime_error("Unterminated string");
        }
        ++pos; // Skip closing quote
        
        return result;
    }

    JsonValue JsonSerializationBackend::ParseValue(const std::string& json, size_t& pos) const {
        SkipWhitespace(json, pos);
        
        if (pos >= json.length()) {
            throw std::runtime_error("Unexpected end of input");
        }
        
        if (json[pos] == '"') {
            return ParseString(json, pos);
        } else if (json.substr(pos, 4) == "true") {
            pos += 4;
            return true;
        } else if (json.substr(pos, 5) == "false") {
            pos += 5;
            return false;
        } else if (json.substr(pos, 4) == "null") {
            pos += 4;
            return std::string(""); // Use empty string for null
        } else if (std::isdigit(json[pos]) || json[pos] == '-') {
            // Parse number
            size_t start = pos;
            if (json[pos] == '-') ++pos;
            
            while (pos < json.length() && std::isdigit(json[pos])) {
                ++pos;
            }
            
            bool isFloat = false;
            if (pos < json.length() && json[pos] == '.') {
                isFloat = true;
                ++pos;
                while (pos < json.length() && std::isdigit(json[pos])) {
                    ++pos;
                }
            }
            
            std::string numStr = json.substr(start, pos - start);
            if (isFloat) {
                return static_cast<float>(std::stod(numStr));
            } else {
                long long value = std::stoll(numStr);
                // Check if the number fits in different integer types
                if (value >= 0) {
                    if (value <= UINT32_MAX) {
                        return static_cast<uint32_t>(value);
                    } else {
                        return static_cast<uint64_t>(value);
                    }
                } else {
                    if (value >= INT32_MIN) {
                        return static_cast<int32_t>(value);
                    } else {
                        return static_cast<int64_t>(value);
                    }
                }
            }
        }
        
        throw std::runtime_error("Invalid JSON value");
    }

} // namespace HE::Serialization
