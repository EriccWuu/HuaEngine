#pragma once

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Rendering/RHI/VertexBufferView.h"
#include "HuaEngine/Serialization/SerializationManager.h"
#include "MeshData.h"
#include <string>

namespace HE::Rendering {
    // Mesh asset class
    class Mesh {
    public:
        Mesh() = default;
        Mesh(const std::string& name, const MeshData& meshData);
        ~Mesh() = default;

        // Get mesh name
        const std::string& GetName() const { return m_Name; }

        void SetName(const std::string& name) { m_Name = name; }

        // Get GPU vertex buffer view (lazy loading)
        Ref<VertexBufferView> GetVertexBufferView();

        // Get raw mesh data
        const MeshData& GetMeshData() const { return m_MeshData; }

        // Set mesh data
        void SetMeshData(const MeshData& meshData);

        // Check if loaded to GPU
        bool IsLoadedToGPU() const { return m_VertexBufferView != nullptr; }

        // Force reload to GPU
        void ReloadToGPU();

        // Release GPU resources
        void UnloadFromGPU();

        // Load mesh data from file
        static Ref<Mesh> LoadFromFile(const std::string& filepath, HE::Serialization::SerializationFormat format = HE::Serialization::SerializationFormat::YAML);

        // Save mesh data to file
        static bool SaveToFile(const Mesh& mesh, const std::string& filepath, HE::Serialization::SerializationFormat format = HE::Serialization::SerializationFormat::YAML);

        // Create basic geometries
        static Ref<Mesh> CreateQuad(const std::string& name = "Quad");
        static Ref<Mesh> CreateCube(const std::string& name = "Cube");
        static Ref<Mesh> CreateSphere(const std::string& name = "Sphere", int segments = 32);

    private:
        std::string m_Name;
        MeshData m_MeshData;
        Ref<VertexBufferView> m_VertexBufferView;  // Lazy loaded GPU resource

        // Create VertexArray from MeshData
        void LoadToGPU();
    };

} // namespace HE::Rendering

namespace HE::Serialization {
    template<>
    struct Serializer<Rendering::Mesh> {
        static void Serialize(SerializationBackend& backend, const std::string& name, const Rendering::Mesh& mesh) {
            if (!name.empty())
                backend.BeginObject(name);

            SerializeValue(backend, "name", mesh.GetName());
            SerializeValue(backend, "mesh_data", mesh.GetMeshData());

            if (!name.empty())
                backend.EndObject();
        }

        static bool Deserialize(SerializationBackend& backend, const std::string& name, Rendering::Mesh& mesh) {
            if (!name.empty())
                backend.BeginObject(name);

            if (!backend.HasField("name") || !backend.HasField("mesh_data")) {
                if (!name.empty())
                    backend.EndObject();
                return false;
            }

            std::string meshName;
            DeserializeValue(backend, "name", meshName);
            mesh.SetName(meshName);

            Rendering::MeshData meshData;
            DeserializeValue(backend, "mesh_data", meshData);
            mesh.SetMeshData(meshData);

            if (!name.empty())
                backend.EndObject();
            return true;
        }
    };

    // Convenience functions for Mesh serialization
    inline bool SaveMesh(const Rendering::Mesh& mesh, const std::string& filename, SerializationFormat format = SerializationFormat::YAML) {
        return SERIALIZE_TO_FILE(mesh, filename, format);
    }

    inline bool LoadMesh(const std::string& filename, Rendering::Mesh& mesh, SerializationFormat format = SerializationFormat::YAML) {
        return DESERIALIZE_FROM_FILE(filename, mesh, format);
    }
} // namespace HE::Serialization
