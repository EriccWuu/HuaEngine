#pragma once

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Rendering/VertexArray.h"
#include "HuaEngine/Serialization/SerializationManager.h"
#include "MeshData.h"
#include <string>

namespace HE {
    // Mesh 资产类
    class Mesh {
    public:
        Mesh() = default;
        Mesh(const std::string& name, const MeshData& meshData);
        ~Mesh() = default;

        // 获取网格名称
        const std::string& GetName() const { return m_Name; }

        void SetName(const std::string& name) { m_Name = name; }
        
        // 获取 VertexArray (延迟加载)
        Ref<VertexArray> GetVertexArray();
        
        // 获取原始网格数据
        const MeshData& GetMeshData() const { return m_MeshData; }
        
        // 设置网格数据
        void SetMeshData(const MeshData& meshData);
        
        // 检查是否已加载到 GPU
        bool IsLoadedToGPU() const { return m_VertexArray != nullptr; }
        
        // 强制重新加载到 GPU
        void ReloadToGPU();
        
        // 释放 GPU 资源
        void UnloadFromGPU();
        
        // 从文件加载网格数据
        static Ref<Mesh> LoadFromFile(const std::string& filepath, SerializationFormat format = SerializationFormat::JSON);
        
        // 保存网格数据到文件
        static bool SaveToFile(const Mesh& mesh, const std::string& filepath, SerializationFormat format = SerializationFormat::JSON);
        
        // 创建基本几何体
        static Ref<Mesh> CreateQuad(const std::string& name = "Quad");
        static Ref<Mesh> CreateCube(const std::string& name = "Cube");
        static Ref<Mesh> CreateSphere(const std::string& name = "Sphere", int segments = 32);

    private:
        std::string m_Name;
        MeshData m_MeshData;
        Ref<VertexArray> m_VertexArray;  // 延迟加载的 GPU 资源
        
        // 从 MeshData 创建 VertexArray
        void LoadToGPU();
    };

    template<>
    struct Serializer<Mesh> {
        static bool Serialize(SerializationBackend& backend, const std::string& name, const Mesh& value) {
            backend.BeginObject(name);
            SerializeValue(backend, "mesh_name", value.GetName());
            SerializeValue(backend, "mesh_data", value.GetMeshData());
            backend.EndObject();
            return true;
        }

        static bool Deserialize(SerializationBackend& backend, const std::string& name, Mesh& value) {
            // backend.BeginObject(name);
            if (!(backend.HasField("mesh_name") && backend.HasField("mesh_data")))
                return false;

            std::string meshName = "";
            DeserializeValue(backend, "mesh_name", meshName);
            value.SetName(meshName);

            auto meshData = MeshData();
            DeserializeValue(backend, "mesh_data", meshData);
            value.SetMeshData(meshData);
            // backend.EndObject();
            return true;
        }
    };
}
