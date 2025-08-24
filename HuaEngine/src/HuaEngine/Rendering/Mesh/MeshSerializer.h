#pragma once

#include "HuaEngine/Serialization/SerializationCore.h"
#include "Module/Rendering/RenderingComponent.h"

namespace HE {
    // 注册 MeshComponent 序列化器
    template<>
    struct Serializer<MeshComponent> {
        static bool Serialize(SerializationBackend& backend, const std::string& name, const MeshComponent& value) {
            // Use reflection serializer
            SerializeValue(backend, name, value);
            return true;
        }

        static bool Deserialize(SerializationBackend& backend, const std::string& name, MeshComponent& value) {
            // Use reflection deserializer
            return DeserializeValue(backend, name, value);
        }
    };
}
