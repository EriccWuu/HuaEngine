#pragma once

#include "HuaEngine/Core/Core.h"
#include "MeshCore.h"
#include <unordered_map>
#include <string>

namespace HE::Rendering {
    // Mesh asset manager (singleton)
    class MeshManager {
    public:
        static MeshManager& Instance() {
            static MeshManager instance;
            return instance;
        }

        // Disable copy constructor and assignment
        MeshManager(const MeshManager&) = delete;
        MeshManager& operator=(const MeshManager&) = delete;

        // Load mesh asset from file
        Ref<Mesh> LoadMesh(const std::string& name, const std::string& filepath);

        // Get loaded mesh asset
        Ref<Mesh> GetMesh(const std::string& name);

        // Register mesh asset (for runtime created meshes)
        void RegisterMesh(const std::string& name, Ref<Mesh> mesh);

        // Unload mesh asset
        void UnloadMesh(const std::string& name);

        // Unload all mesh assets
        void UnloadAllMeshes();

        // Get list of loaded mesh names
        std::vector<std::string> GetLoadedMeshNames() const;

        // Check if mesh is loaded
        bool IsMeshLoaded(const std::string& name) const;

        // Preload default meshes
        void LoadDefaultMeshes();

        // Release unused GPU resources
        void ReleaseUnusedGPUResources();

    private:
        MeshManager() = default;
        ~MeshManager() = default;

        std::unordered_map<std::string, Ref<Mesh>> m_LoadedMeshes;
        std::unordered_map<std::string, std::string> m_MeshPaths;  // Mesh name -> file path mapping
    };

    // Convenience global functions
    inline Ref<Mesh> GetMesh(const std::string& name) {
        return MeshManager::Instance().GetMesh(name);
    }

    inline Ref<Mesh> LoadMesh(const std::string& name, const std::string& filepath) {
        return MeshManager::Instance().LoadMesh(name, filepath);
    }
}
