#include "enginepch.h"
#include "MaterialSerializer.h"
#include "HuaEngine/Core/Log.h"

namespace HE::Rendering {

    void MaterialParameterSerializer::Serialize(HE::Serialization::SerializationBackend& backend, const std::string& name, const MaterialParameterValue& value) {
        std::visit([&](auto&& val) {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, Ref<Texture2D>>) {
                std::string texturePath = val ? val->GetPath() : "";
                backend.Serialize(name, texturePath);
            }
            else {
                HE::Serialization::SerializeValue(backend, name, val);
            }
        }, value);
    }

    bool MaterialParameterSerializer::Deserialize(HE::Serialization::SerializationBackend& backend, const std::string& name, MaterialParameterValue& value, MaterialParameterType type) {
        if (!backend.HasField(name)) {
            return false;
        }

        try {
            switch (type) {
                case MaterialParameterType::Int: {
                    int intValue;
                    if (backend.Deserialize(name, intValue)) {
                        value = intValue;
                        return true;
                    }
                    break;
                }
                
                case MaterialParameterType::Float: {
                    float floatValue;
                    if (backend.Deserialize(name, floatValue)) {
                        value = floatValue;
                        return true;
                    }
                    break;
                }
                
                case MaterialParameterType::Vec2: {
                    glm::vec2 vec2Value;
                    HE::Serialization::Serializer<glm::vec2>::Deserialize(backend, name, vec2Value);
                    value = vec2Value;
                    return true;
                }
                
                case MaterialParameterType::Vec3: {
                    glm::vec3 vec3Value;
                    HE::Serialization::Serializer<glm::vec3>::Deserialize(backend, name, vec3Value);
                    value = vec3Value;
                    return true;
                }
                
                case MaterialParameterType::Vec4: {
                    glm::vec4 vec4Value;
                    HE::Serialization::Serializer<glm::vec4>::Deserialize(backend, name, vec4Value);
                    value = vec4Value;
                    return true;
                }
                
                case MaterialParameterType::Mat3: {
                    glm::mat3 mat3Value;
                    HE::Serialization::Serializer<glm::mat3>::Deserialize(backend, name, mat3Value);
                    value = mat3Value;
                    return true;
                }
                
                case MaterialParameterType::Mat4: {
                    glm::mat4 mat4Value;
                    HE::Serialization::Serializer<glm::mat4>::Deserialize(backend, name, mat4Value);
                    value = mat4Value;
                    return true;
                }
                
                case MaterialParameterType::Texture2D: {
                    std::string texturePath;
                    if (backend.Deserialize(name, texturePath)) {
                        value = Texture2D::Create(texturePath);
                        return true;
                    }
                    break;
                }
                
                case MaterialParameterType::IntArray: {
                    if (backend.HasField(name)) {
                        size_t arraySize = backend.GetArraySize(name);
                        backend.BeginArray(name);
                        std::vector<int> intArray(arraySize);
                        
                        for (size_t i = 0; i < arraySize; ++i) {
                            backend.BeginArrayElement(i);
                            backend.Deserialize("", intArray[i]);
                            backend.EndArrayElement();
                        }
                        backend.EndArray();
                        
                        value = intArray;
                        return true;
                    }
                    break;
                }
                
                case MaterialParameterType::FloatArray: {
                    if (backend.HasField(name)) {
                        size_t arraySize = backend.GetArraySize(name);
                        backend.BeginArray(name);
                        std::vector<float> floatArray(arraySize);
                        
                        for (size_t i = 0; i < arraySize; ++i) {
                            backend.BeginArrayElement(i);
                            backend.Deserialize("", floatArray[i]);
                            backend.EndArrayElement();
                        }
                        backend.EndArray();
                        
                        value = floatArray;
                        return true;
                    }
                    break;
                }
                
                default:
                    HE_CORE_WARN("Unknown material parameter type for deserialization");
                    return false;
            }
        }
        catch (const std::exception& e) {
            HE_CORE_ERROR("Error deserializing material parameter: {0}", e.what());
            return false;
        }

        return false;
    }

    MaterialParameterType MaterialParameterSerializer::StringToParameterType(const std::string& typeStr) {
        if (typeStr == "Int") return MaterialParameterType::Int;
        if (typeStr == "Float") return MaterialParameterType::Float;
        if (typeStr == "Vec2") return MaterialParameterType::Vec2;
        if (typeStr == "Vec3") return MaterialParameterType::Vec3;
        if (typeStr == "Vec4") return MaterialParameterType::Vec4;
        if (typeStr == "Mat3") return MaterialParameterType::Mat3;
        if (typeStr == "Mat4") return MaterialParameterType::Mat4;
        if (typeStr == "Texture2D") return MaterialParameterType::Texture2D;
        if (typeStr == "TextureCube") return MaterialParameterType::TextureCube;
        if (typeStr == "IntArray") return MaterialParameterType::IntArray;
        if (typeStr == "FloatArray") return MaterialParameterType::FloatArray;
        
        HE_CORE_WARN("Unknown material parameter type string: {0}", typeStr);
        return MaterialParameterType::Float; // 默认类型
    }

    std::string MaterialParameterSerializer::ParameterTypeToString(MaterialParameterType type) {
        switch (type) {
            case MaterialParameterType::Int: return "Int";
            case MaterialParameterType::Float: return "Float";
            case MaterialParameterType::Vec2: return "Vec2";
            case MaterialParameterType::Vec3: return "Vec3";
            case MaterialParameterType::Vec4: return "Vec4";
            case MaterialParameterType::Mat3: return "Mat3";
            case MaterialParameterType::Mat4: return "Mat4";
            case MaterialParameterType::Texture2D: return "Texture2D";
            case MaterialParameterType::TextureCube: return "TextureCube";
            case MaterialParameterType::IntArray: return "IntArray";
            case MaterialParameterType::FloatArray: return "FloatArray";
            default: return "Unknown";
        }
    }

}
