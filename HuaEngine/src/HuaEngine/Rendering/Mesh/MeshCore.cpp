#include "enginepch.h"
#include "Mesh.h"
#include "HuaEngine/Serialization/Serialization.h"
#include "HuaEngine/Serialization/SerializationManager.h"

namespace HE::Rendering {
    Mesh::Mesh(const std::string& name, const MeshData& meshData)
        : m_Name(name), m_MeshData(meshData) {
    }

    Ref<VertexBufferView> Mesh::GetVertexBufferView() {
        if (!m_VertexBufferView && m_MeshData.IsValid()) {
            LoadToGPU();
        }
        return m_VertexBufferView;
    }

    void Mesh::SetMeshData(const MeshData& meshData) {
        m_MeshData = meshData;
        // Clear existing GPU resource, reload on next access
        m_VertexBufferView = nullptr;
    }

    void Mesh::ReloadToGPU() {
        UnloadFromGPU();
        LoadToGPU();
    }

    void Mesh::UnloadFromGPU() {
        m_VertexBufferView = nullptr;  // Smart pointer auto-releases resources
    }

    void Mesh::LoadToGPU() {
        if (!m_MeshData.IsValid()) {
            HE_CORE_WARN("Mesh::LoadToGPU - Invalid mesh data for '{}'", m_Name);
            return;
        }

        auto vertexArray = m_MeshData.ToVertexArray();
        m_VertexBufferView = vertexArray ? vertexArray->GetVertexBufferView() : nullptr;
        if (m_VertexBufferView) {
            HE_CORE_INFO("Mesh '{}' loaded to GPU successfully", m_Name);
        } else {
            HE_CORE_ERROR("Failed to load mesh '{}' to GPU", m_Name);
        }
    }

    Ref<Mesh> Mesh::LoadFromFile(const std::string& filepath, HE::Serialization::SerializationFormat format) {
        Mesh mesh;

        if (!HE::Serialization::LoadMesh(filepath, mesh, format)) {
            HE_CORE_ERROR("Failed to load mesh file: {}", filepath);
            return nullptr;
        }

        HE_CORE_INFO("Loaded mesh from file: {}", filepath);
        return CreateRef<Mesh>(mesh);
    }

    bool Mesh::SaveToFile(const Mesh& mesh, const std::string& filepath, HE::Serialization::SerializationFormat format) {
        bool success = HE::Serialization::SaveMesh(mesh, filepath, format);
        if (success) {
            HE_CORE_INFO("Saved mesh '{}' to file: {}", mesh.m_Name, filepath);
        } else {
            HE_CORE_ERROR("Failed to save mesh '{}' to file: {}", mesh.m_Name, filepath);
        }
        return success;
    }

    Ref<Mesh> Mesh::CreateQuad(const std::string& name) {
        // Create quad vertex data
        std::vector<float> vertices = {
            -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,  // Bottom-left
             0.5f, -0.5f, 0.0f, 1.0f, 0.0f,  // Bottom-right
             0.5f,  0.5f, 0.0f, 1.0f, 1.0f,  // Top-right
            -0.5f,  0.5f, 0.0f, 0.0f, 1.0f   // Top-left
        };

        std::vector<uint32_t> indices = {
            0, 1, 2,  // First triangle
            2, 3, 0   // Second triangle
        };

        // Create layout
        SerializableBufferLayout layout;
        layout.Elements = {
            SerializableBufferElement{static_cast<uint8_t>(ShaderDataType::Float3), "a_Position", 12, 0, false},
            SerializableBufferElement{static_cast<uint8_t>(ShaderDataType::Float2), "a_TexCoord", 8, 12, false}
        };
        layout.Stride = 20;

        MeshData meshData;
        meshData.VertexData = vertices;
        meshData.IndexData = indices;
        meshData.Layout = layout;

        return CreateRef<Mesh>(name, meshData);
    }

    Ref<Mesh> Mesh::CreateCube(const std::string& name) {
        // Create cube vertex data (simplified version, position and texcoord only)
        std::vector<float> vertices = {
            // Front face
            -0.5f, -0.5f,  0.5f, 0.0f, 0.0f,
             0.5f, -0.5f,  0.5f, 1.0f, 0.0f,
             0.5f,  0.5f,  0.5f, 1.0f, 1.0f,
            -0.5f,  0.5f,  0.5f, 0.0f, 1.0f,

            // Back face
            -0.5f, -0.5f, -0.5f, 1.0f, 0.0f,
             0.5f, -0.5f, -0.5f, 0.0f, 0.0f,
             0.5f,  0.5f, -0.5f, 0.0f, 1.0f,
            -0.5f,  0.5f, -0.5f, 1.0f, 1.0f
        };

        std::vector<uint32_t> indices = {
            // Front face
            0, 1, 2, 2, 3, 0,
            // Back face
            4, 5, 6, 6, 7, 4,
            // Left face
            7, 3, 0, 0, 4, 7,
            // Right face
            1, 5, 6, 6, 2, 1,
            // Top face
            3, 2, 6, 6, 7, 3,
            // Bottom face
            0, 1, 5, 5, 4, 0
        };

        SerializableBufferLayout layout;
        layout.Elements = {
            SerializableBufferElement{static_cast<uint8_t>(ShaderDataType::Float3), "a_Position", 12, 0, false},
            SerializableBufferElement{static_cast<uint8_t>(ShaderDataType::Float2), "a_TexCoord", 8, 12, false}
        };
        layout.Stride = 20;

        MeshData meshData;
        meshData.VertexData = vertices;
        meshData.IndexData = indices;
        meshData.Layout = layout;

        return CreateRef<Mesh>(name, meshData);
    }

    Ref<Mesh> Mesh::CreateSphere(const std::string& name, int segments) {
        // Simplified sphere vertex data implementation
        // This is just an example, actual implementation would be more complex
        std::vector<float> vertices;
        std::vector<uint32_t> indices;

        const float PI = 3.14159265359f;

        // Generate sphere vertices
        for (int lat = 0; lat <= segments; ++lat) {
            float theta = lat * PI / segments;
            float sinTheta = sin(theta);
            float cosTheta = cos(theta);
            
            for (int lon = 0; lon <= segments; ++lon) {
                float phi = lon * 2 * PI / segments;
                float sinPhi = sin(phi);
                float cosPhi = cos(phi);
                
                float x = cosPhi * sinTheta;
                float y = cosTheta;
                float z = sinPhi * sinTheta;
                float u = 1.0f - (float)lon / segments;
                float v = 1.0f - (float)lat / segments;
                
                vertices.insert(vertices.end(), {x * 0.5f, y * 0.5f, z * 0.5f, u, v});
            }
        }

        // Generate sphere indices
        for (int lat = 0; lat < segments; ++lat) {
            for (int lon = 0; lon < segments; ++lon) {
                int first = (lat * (segments + 1)) + lon;
                int second = first + segments + 1;
                
                indices.insert(indices.end(), {static_cast<uint32_t>(first), static_cast<uint32_t>(second), static_cast<uint32_t>(first + 1)});
                indices.insert(indices.end(), {static_cast<uint32_t>(second), static_cast<uint32_t>(second + 1), static_cast<uint32_t>(first + 1)});
            }
        }

        SerializableBufferLayout layout;
        layout.Elements = {
            SerializableBufferElement{static_cast<uint8_t>(ShaderDataType::Float3), "a_Position", 12, 0, false},
            SerializableBufferElement{static_cast<uint8_t>(ShaderDataType::Float2), "a_TexCoord", 8, 12, false}
        };
        layout.Stride = 20;

        MeshData meshData;
        meshData.VertexData = vertices;
        meshData.IndexData = indices;
        meshData.Layout = layout;

        return CreateRef<Mesh>(name, meshData);
    }
}
