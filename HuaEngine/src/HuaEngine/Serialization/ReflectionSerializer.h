#pragma once

#include "Serialization.h"
#include "HuaEngine/Reflection/Reflection.h"
#include "glm/glm.hpp"

namespace HE {

    // Default serializer using reflection
    template<typename T>
    struct Serializer {
        static void Serialize(SerializationBackend& backend, const std::string& name, const T& obj) {
            if constexpr (std::is_arithmetic_v<T> || std::is_same_v<T, std::string>) {
                // For basic types, use direct serialization
                SerializeValue(backend, name, obj);
            } else {
                // For complex types, use reflection
                if (!name.empty()) {
                    backend.BeginObject(name);
                } else {
                    backend.BeginObject();
                }

                auto fieldInfo = Refl::reflect<T>();
                fieldInfo.visit_fields([&](auto&& field) {
                    const auto& fieldValue = field.GetValue(&obj);
                    SerializeValue(backend, std::string(field.name().data(), field.name().size()), fieldValue);
                });

                backend.EndObject();
            }
        }

        static bool Deserialize(SerializationBackend& backend, const std::string& name, T& obj) {
            if constexpr (std::is_arithmetic_v<T> || std::is_same_v<T, std::string>) {
                // For basic types, use direct deserialization
                return DeserializeValue(backend, name, obj);
            } else {
                // For complex types, use reflection
                if (!name.empty()) {
                    if (!backend.HasField(name)) return false;
                    backend.BeginObject(name);
                } else {
                    backend.BeginObject();
                }

                bool success = true;
                auto fieldInfo = Refl::reflect<T>();
                fieldInfo.visit_fields([&](auto&& field) {
                    std::string fieldName(field.name().data(), field.name().size());
                    
                    // Create a temporary variable to hold the deserialized value
                    using FieldType = std::remove_cv_t<std::remove_reference_t<
                        decltype(field.GetValue(&obj))>>;
                    FieldType tempValue{};
                    
                    if (DeserializeValue(backend, fieldName, tempValue)) {
                        // Use direct assignment through offset rather than SetValue
                        auto* fieldPtr = reinterpret_cast<FieldType*>(
                            reinterpret_cast<char*>(&obj) + field.offset());
                        *fieldPtr = tempValue;
                    } else {
                        // For optional fields, we might not want to fail completely
                        // success = false;
                    }
                });

                backend.EndObject();
                return success;
            }
        }
    };

    // Specialization for std::vector
    template<typename T>
    struct Serializer<std::vector<T>> {
        static void Serialize(SerializationBackend& backend, const std::string& name, const std::vector<T>& vec) {
            SerializeArray(backend, name, vec);
        }

        static bool Deserialize(SerializationBackend& backend, const std::string& name, std::vector<T>& vec) {
            return DeserializeArray(backend, name, vec);
        }
    };

    // Specialization for glm::vec3 (commonly used in engines)
    template<>
    struct Serializer<glm::vec3> {
        static void Serialize(SerializationBackend& backend, const std::string& name, const glm::vec3& vec) {
            backend.BeginObject(name);
            backend.Serialize("x", vec.x);
            backend.Serialize("y", vec.y);
            backend.Serialize("z", vec.z);
            backend.EndObject();
        }

        static bool Deserialize(SerializationBackend& backend, const std::string& name, glm::vec3& vec) {
            if (!backend.HasField(name)) return false;
            
            backend.BeginObject(name);
            bool success = true;
            success &= backend.Deserialize("x", vec.x);
            success &= backend.Deserialize("y", vec.y);
            success &= backend.Deserialize("z", vec.z);
            backend.EndObject();
            return success;
        }
    };

    // Specialization for glm::vec4
    template<>
    struct Serializer<glm::vec4> {
        static void Serialize(SerializationBackend& backend, const std::string& name, const glm::vec4& vec) {
            backend.BeginObject(name);
            backend.Serialize("x", vec.x);
            backend.Serialize("y", vec.y);
            backend.Serialize("z", vec.z);
            backend.Serialize("w", vec.w);
            backend.EndObject();
        }

        static bool Deserialize(SerializationBackend& backend, const std::string& name, glm::vec4& vec) {
            if (!backend.HasField(name)) return false;
            
            backend.BeginObject(name);
            bool success = true;
            success &= backend.Deserialize("x", vec.x);
            success &= backend.Deserialize("y", vec.y);
            success &= backend.Deserialize("z", vec.z);
            success &= backend.Deserialize("w", vec.w);
            backend.EndObject();
            return success;
        }
    };

    // Specialization for glm::mat4 (if needed)
    template<>
    struct Serializer<glm::mat4> {
        static void Serialize(SerializationBackend& backend, const std::string& name, const glm::mat4& mat) {
            backend.BeginArray(name, 16);
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 4; ++j) {
                    backend.BeginArrayElement(i * 4 + j);
                    backend.Serialize("", mat[i][j]);
                    backend.EndArrayElement();
                }
            }
            backend.EndArray();
        }

        static bool Deserialize(SerializationBackend& backend, const std::string& name, glm::mat4& mat) {
            if (!backend.HasField(name) || backend.GetArraySize(name) != 16) return false;
            
            backend.BeginArray(name);
            bool success = true;
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 4; ++j) {
                    backend.BeginArrayElement(i * 4 + j);
                    success &= backend.Deserialize("", mat[i][j]);
                    backend.EndArrayElement();
                }
            }
            backend.EndArray();
            return success;
        }
    };

}
