#pragma once

#include "HuaEngine/Serialization/Serialization.h"
#include "Module/Rendering/RenderingComponent.h"

namespace HE {
    // MeshComponent 序列化器（基于资产路径）
    class MeshComponentSerializer {
    public:
        static bool Serialize(SerializationBackend& backend, const std::string& name, const MeshComponent& meshComponent) {
            // 直接序列化资产名称
            SerializeValue(backend, name, meshComponent);
            return true;
        }

        static bool Deserialize(SerializationBackend& backend, const std::string& name, MeshComponent& meshComponent) {
            // 直接反序列化资产名称，VertexArray 会在需要时延迟加载
            return DeserializeValue(backend, name, meshComponent);
        }
    };

    // 注册 MeshComponent 序列化器
    template<>
    struct Serializer<MeshComponent> {
        static bool Serialize(SerializationBackend& backend, const std::string& name, const MeshComponent& value) {
            return MeshComponentSerializer::Serialize(backend, name, value);
        }

        static bool Deserialize(SerializationBackend& backend, const std::string& name, MeshComponent& value) {
            return MeshComponentSerializer::Deserialize(backend, name, value);
        }
    };

    // Mesh 资产序列化器
    class MeshAssetSerializer {
    public:
        static bool SaveMeshAsset(const std::string& filepath, Ref<Mesh> mesh) {
            if (!mesh) return false;
            return mesh->SaveToFile(filepath);
        }

        static Ref<Mesh> LoadMeshAsset(const std::string& filepath) {
            return Mesh::LoadFromFile(filepath);
        }
    };
}
