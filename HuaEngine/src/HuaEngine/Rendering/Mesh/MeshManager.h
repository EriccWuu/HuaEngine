#pragma once

#include "HuaEngine/Core/Core.h"
#include "MeshCore.h"
#include <unordered_map>
#include <string>

namespace HE {
    // Mesh 资产管理器 (单例模式)
    class MeshManager {
    public:
        static MeshManager& Instance() {
            static MeshManager instance;
            return instance;
        }

        // 禁用拷贝构造和赋值
        MeshManager(const MeshManager&) = delete;
        MeshManager& operator=(const MeshManager&) = delete;

        // 从文件加载网格资产
        Ref<Mesh> LoadMesh(const std::string& name, const std::string& filepath);
        
        // 获取已加载的网格资产
        Ref<Mesh> GetMesh(const std::string& name);
        
        // 注册网格资产（用于运行时创建的网格）
        void RegisterMesh(const std::string& name, Ref<Mesh> mesh);
        
        // 卸载网格资产
        void UnloadMesh(const std::string& name);
        
        // 卸载所有网格资产
        void UnloadAllMeshes();
        
        // 获取已加载的网格列表
        std::vector<std::string> GetLoadedMeshNames() const;
        
        // 检查网格是否已加载
        bool IsMeshLoaded(const std::string& name) const;
        
        // 预加载默认网格
        void LoadDefaultMeshes();
        
        // 释放未使用的 GPU 资源
        void ReleaseUnusedGPUResources();

    private:
        MeshManager() = default;
        ~MeshManager() = default;

        std::unordered_map<std::string, Ref<Mesh>> m_LoadedMeshes;
        std::unordered_map<std::string, std::string> m_MeshPaths;  // 网格名称 -> 文件路径映射
    };

    // 便捷的全局函数
    inline Ref<Mesh> GetMesh(const std::string& name) {
        return MeshManager::Instance().GetMesh(name);
    }

    inline Ref<Mesh> LoadMesh(const std::string& name, const std::string& filepath) {
        return MeshManager::Instance().LoadMesh(name, filepath);
    }
}
