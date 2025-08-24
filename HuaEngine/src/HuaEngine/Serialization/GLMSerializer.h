#pragma once

#include "Serialization.h"
#include "glm/glm.hpp"

namespace HE::Serialization {
    // Specialization for glm::vec2 (commonly used in engines)
    template<>
    struct Serializer<glm::vec2> {
        static void Serialize(SerializationBackend& backend, const std::string& name, const glm::vec2& value) {
            backend.BeginObject(name);
            backend.Serialize("x", value.x);
            backend.Serialize("y", value.y);
            backend.EndObject();
        }

        static void Deserialize(SerializationBackend& backend, const std::string& name, glm::vec2& value) {
            if (backend.HasField(name)) {
                backend.BeginObject(name);
                backend.Deserialize("x", value.x);
                backend.Deserialize("y", value.y);
                backend.EndObject();
            }
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

    // Specialization for glm::mat3 (if needed)
    template<>
    struct Serializer<glm::mat3> {
        static void Serialize(SerializationBackend& backend, const std::string& name, const glm::mat3& value) {
            backend.BeginArray(name, 9);
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 3; ++j) {
                    backend.BeginArrayElement(i * 3 + j);
                    backend.Serialize("", value[i][j]);
                    backend.EndArrayElement();
                }
            }
            backend.EndArray();
        }

        static void Deserialize(SerializationBackend& backend, const std::string& name, glm::mat3& value) {
            if (backend.HasField(name)) {
                backend.BeginArray(name);
                for (int i = 0; i < 3; ++i) {
                    for (int j = 0; j < 3; ++j) {
                        backend.BeginArrayElement(i * 3 + j);
                        backend.Deserialize("", value[i][j]);
                        backend.EndArrayElement();
                    }
                }
                backend.EndArray();
            }
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

} // namespace HE::Serialization
