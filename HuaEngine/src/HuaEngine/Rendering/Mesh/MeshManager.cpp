#include "enginepch.h"
#include "MeshManager.h"

namespace HE::Rendering {
    MeshManager& MeshManager::Instance() {
        static MeshManager instance;
        return instance;
    }

    Ref<Mesh> MeshManager::LoadMesh(const std::string& name, const std::string& filepath) {
        // Check if already loaded
        auto it = m_LoadedMeshes.find(name);
        if (it != m_LoadedMeshes.end()) {
            HE_CORE_INFO("Mesh '{}' already loaded, returning existing instance", name);
            return it->second;
        }

        // Load from file
        auto mesh = Mesh::LoadFromFile(filepath);
        if (!mesh) {
            HE_CORE_ERROR("Failed to load mesh '{}' from file: {}", name, filepath);
            return nullptr;
        }

        // Register to manager
        m_LoadedMeshes[name] = mesh;
        m_MeshPaths[name] = filepath;
        
        HE_CORE_INFO("Mesh '{}' loaded and registered successfully", name);
        return mesh;
    }

    Ref<Mesh> MeshManager::GetMesh(const std::string& name) {
        auto it = m_LoadedMeshes.find(name);
        if (it != m_LoadedMeshes.end()) {
            return it->second;
        }

        HE_CORE_WARN("Mesh '{}' not found in loaded meshes", name);
        return nullptr;
    }

    void MeshManager::RegisterMesh(const std::string& name, Ref<Mesh> mesh) {
        if (!mesh) {
            HE_CORE_WARN("Attempted to register null mesh with name '{}'", name);
            return;
        }

        m_LoadedMeshes[name] = mesh;
        HE_CORE_INFO("Mesh '{}' registered successfully", name);
    }

    void MeshManager::UnloadMesh(const std::string& name) {
        auto it = m_LoadedMeshes.find(name);
        if (it != m_LoadedMeshes.end()) {
            // Release GPU resources
            it->second->UnloadFromGPU();
            m_LoadedMeshes.erase(it);
            m_MeshPaths.erase(name);
            HE_CORE_INFO("Mesh '{}' unloaded successfully", name);
        } else {
            HE_CORE_WARN("Attempted to unload non-existent mesh '{}'", name);
        }
    }

    void MeshManager::UnloadAllMeshes() {
        for (auto& [name, mesh] : m_LoadedMeshes) {
            mesh->UnloadFromGPU();
        }
        m_LoadedMeshes.clear();
        m_MeshPaths.clear();
        HE_CORE_INFO("All meshes unloaded");
    }

    std::vector<std::string> MeshManager::GetLoadedMeshNames() const {
        std::vector<std::string> names;
        names.reserve(m_LoadedMeshes.size());
        
        for (const auto& [name, mesh] : m_LoadedMeshes) {
            names.push_back(name);
        }
        
        return names;
    }

    bool MeshManager::IsMeshLoaded(const std::string& name) const {
        return m_LoadedMeshes.find(name) != m_LoadedMeshes.end();
    }

    void MeshManager::LoadDefaultMeshes() {
        HE_CORE_INFO("Loading default meshes...");

        // Create and register default geometries
        RegisterMesh("Quad", Mesh::CreateQuad("Quad"));
        RegisterMesh("Cube", Mesh::CreateCube("Cube"));
        RegisterMesh("Sphere", Mesh::CreateSphere("Sphere", 32));
        
        HE_CORE_INFO("Default meshes loaded: Quad, Cube, Sphere");
    }

    void MeshManager::ReleaseUnusedGPUResources() {
        int releasedCount = 0;
        for (auto& [name, mesh] : m_LoadedMeshes) {
            // If mesh is only referenced by manager (use_count == 1), release GPU resources
            if (mesh.use_count() == 1 && mesh->IsLoadedToGPU()) {
                mesh->UnloadFromGPU();
                releasedCount++;
                HE_CORE_INFO("Released GPU resources for unused mesh '{}'", name);
            }
        }
        
        if (releasedCount > 0) {
            HE_CORE_INFO("Released GPU resources for {} unused meshes", releasedCount);
        }
    }
}
