#pragma once

#include "HuaEngine/Serialization/Serialization.h"
#include "glm/glm.hpp"

namespace HE {

    // GLM vec2 序列化
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

    // GLM vec3 序列化
    template<>
    struct Serializer<glm::vec3> {
        static void Serialize(SerializationBackend& backend, const std::string& name, const glm::vec3& value) {
            backend.BeginObject(name);
            backend.Serialize("x", value.x);
            backend.Serialize("y", value.y);
            backend.Serialize("z", value.z);
            backend.EndObject();
        }

        static void Deserialize(SerializationBackend& backend, const std::string& name, glm::vec3& value) {
            if (backend.HasField(name)) {
                backend.BeginObject(name);
                backend.Deserialize("x", value.x);
                backend.Deserialize("y", value.y);
                backend.Deserialize("z", value.z);
                backend.EndObject();
            }
        }
    };

    // GLM vec4 序列化
    template<>
    struct Serializer<glm::vec4> {
        static void Serialize(SerializationBackend& backend, const std::string& name, const glm::vec4& value) {
            backend.BeginObject(name);
            backend.Serialize("x", value.x);
            backend.Serialize("y", value.y);
            backend.Serialize("z", value.z);
            backend.Serialize("w", value.w);
            backend.EndObject();
        }

        static void Deserialize(SerializationBackend& backend, const std::string& name, glm::vec4& value) {
            if (backend.HasField(name)) {
                backend.BeginObject(name);
                backend.Deserialize("x", value.x);
                backend.Deserialize("y", value.y);
                backend.Deserialize("z", value.z);
                backend.Deserialize("w", value.w);
                backend.EndObject();
            }
        }
    };

    // GLM mat3 序列化
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

    // GLM mat4 序列化
    template<>
    struct Serializer<glm::mat4> {
        static void Serialize(SerializationBackend& backend, const std::string& name, const glm::mat4& value) {
            backend.BeginArray(name, 16);
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 4; ++j) {
                    backend.BeginArrayElement(i * 4 + j);
                    backend.Serialize("", value[i][j]);
                    backend.EndArrayElement();
                }
            }
            backend.EndArray();
        }

        static void Deserialize(SerializationBackend& backend, const std::string& name, glm::mat4& value) {
            if (backend.HasField(name)) {
                backend.BeginArray(name);
                for (int i = 0; i < 4; ++i) {
                    for (int j = 0; j < 4; ++j) {
                        backend.BeginArrayElement(i * 4 + j);
                        backend.Deserialize("", value[i][j]);
                        backend.EndArrayElement();
                    }
                }
                backend.EndArray();
            }
        }
    };

}
