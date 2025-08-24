#pragma once

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Rendering/VertexArray.h"
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
        static Ref<Mesh> LoadFromFile(const std::string& filepath);
        
        // 保存网格数据到文件
        bool SaveToFile(const std::string& filepath) const;
        
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
}
