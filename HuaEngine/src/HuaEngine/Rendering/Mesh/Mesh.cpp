#include "enginepch.h"
#include "Mesh.h"
#include "HuaEngine/Serialization/Serialization.h"
#include "HuaEngine/Serialization/SerializationManager.h"

namespace HE {
    Mesh::Mesh(const std::string& name, const MeshData& meshData)
        : m_Name(name), m_MeshData(meshData) {
    }

    Ref<VertexArray> Mesh::GetVertexArray() {
        if (!m_VertexArray && m_MeshData.IsValid()) {
            LoadToGPU();
        }
        return m_VertexArray;
    }

    void Mesh::SetMeshData(const MeshData& meshData) {
        m_MeshData = meshData;
        // 清除现有的 GPU 资源，下次访问时重新加载
        m_VertexArray = nullptr;
    }

    void Mesh::ReloadToGPU() {
        UnloadFromGPU();
        LoadToGPU();
    }

    void Mesh::UnloadFromGPU() {
        m_VertexArray = nullptr;  // 智能指针会自动释放资源
    }

    void Mesh::LoadToGPU() {
        if (!m_MeshData.IsValid()) {
            HE_CORE_WARN("Mesh::LoadToGPU - Invalid mesh data for '{}'", m_Name);
            return;
        }

        m_VertexArray = m_MeshData.ToVertexArray();
        if (m_VertexArray) {
            HE_CORE_INFO("Mesh '{}' loaded to GPU successfully", m_Name);
        } else {
            HE_CORE_ERROR("Failed to load mesh '{}' to GPU", m_Name);
        }
    }

    Ref<Mesh> Mesh::LoadFromFile(const std::string& filepath, SerializationFormat format) {
        MeshData meshData;
        
        // 使用 SerializationManager 来反序列化
        if (!SerializationManager::Instance().DeserializeFromFile(filepath, meshData, format)) {
            HE_CORE_ERROR("Failed to load mesh file: {}", filepath);
            return nullptr;
        }

        auto mesh = CreateRef<Mesh>("LoadedMesh", meshData);
        HE_CORE_INFO("Loaded mesh from file: {}", filepath);
        return mesh;
    }

    bool Mesh::SaveToFile(const Mesh& mesh, const std::string& filepath, SerializationFormat format) {
        // 使用 SerializationManager 来序列化
        bool success = SerializationManager::Instance().SerializeToFile(mesh.m_MeshData, filepath, format);
        if (success) {
            HE_CORE_INFO("Saved mesh '{}' to file: {}", mesh.m_Name, filepath);
        } else {
            HE_CORE_ERROR("Failed to save mesh '{}' to file: {}", mesh.m_Name, filepath);
        }
        return success;
    }

    Ref<Mesh> Mesh::CreateQuad(const std::string& name) {
        // 创建四边形顶点数据
        std::vector<float> vertices = {
            -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,  // 左下
             0.5f, -0.5f, 0.0f, 1.0f, 0.0f,  // 右下
             0.5f,  0.5f, 0.0f, 1.0f, 1.0f,  // 右上
            -0.5f,  0.5f, 0.0f, 0.0f, 1.0f   // 左上
        };
        
        std::vector<uint32_t> indices = {
            0, 1, 2,  // 第一个三角形
            2, 3, 0   // 第二个三角形
        };

        // 创建布局
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
        // 创建立方体顶点数据 (简化版本，只包含位置和纹理坐标)
        std::vector<float> vertices = {
            // 前面
            -0.5f, -0.5f,  0.5f, 0.0f, 0.0f,
             0.5f, -0.5f,  0.5f, 1.0f, 0.0f,
             0.5f,  0.5f,  0.5f, 1.0f, 1.0f,
            -0.5f,  0.5f,  0.5f, 0.0f, 1.0f,
            
            // 后面
            -0.5f, -0.5f, -0.5f, 1.0f, 0.0f,
             0.5f, -0.5f, -0.5f, 0.0f, 0.0f,
             0.5f,  0.5f, -0.5f, 0.0f, 1.0f,
            -0.5f,  0.5f, -0.5f, 1.0f, 1.0f
        };
        
        std::vector<uint32_t> indices = {
            // 前面
            0, 1, 2, 2, 3, 0,
            // 后面
            4, 5, 6, 6, 7, 4,
            // 左面
            7, 3, 0, 0, 4, 7,
            // 右面
            1, 5, 6, 6, 2, 1,
            // 上面
            3, 2, 6, 6, 7, 3,
            // 下面
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
        // 创建球体顶点数据的简化实现
        // 这里只是一个示例，实际实现会更复杂
        std::vector<float> vertices;
        std::vector<uint32_t> indices;
        
        const float PI = 3.14159265359f;
        
        // 生成球体顶点
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
        
        // 生成球体索引
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
