#include "enginepch.h"
#include "MeshData.h"
#include "Platform/OpenGL/OpenGLVertexBuffer.h"
#include "Platform/OpenGL/OpenGLIndexBuffer.h"
#include "glad/glad.h"

namespace HE::Rendering {
    MeshData MeshData::FromVertexArray(const Ref<VertexArray>& vertexArray) {
        if (!vertexArray) {
            HE_CORE_WARN("MeshData::FromVertexArray - VertexArray is null");
            return {};
        }

        MeshData meshData;
        
        // 获取顶点缓冲区数据
        const auto& vertexBuffers = vertexArray->GetVertexBuffers();
        if (vertexBuffers.empty()) {
            HE_CORE_WARN("MeshData::FromVertexArray - No vertex buffers found");
            return {};
        }

        // 目前只支持单个顶点缓冲区
        const auto& vertexBuffer = vertexBuffers[0];
        if (!vertexBuffer) {
            HE_CORE_WARN("MeshData::FromVertexArray - First vertex buffer is null");
            return {};
        }

        // 保存布局信息
        meshData.Layout = SerializableBufferLayout(vertexBuffer->GetLayout());

        // 从 OpenGL 缓冲区读取顶点数据
        auto openglBuffer = std::dynamic_pointer_cast<OpenGLVertexBuffer>(vertexBuffer);
        if (!openglBuffer) {
            HE_CORE_WARN("MeshData::FromVertexArray - Failed to cast to OpenGLVertexBuffer");
            return {};
        }

        // 绑定并读取顶点数据
        openglBuffer->Bind();
        
        // 获取缓冲区大小
        GLint bufferSize;
        glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &bufferSize);
        
        if (bufferSize <= 0) {
            HE_CORE_WARN("MeshData::FromVertexArray - Invalid buffer size: {}", bufferSize);
            return {};
        }

        // 分配内存并读取数据
        meshData.VertexData.resize(bufferSize / sizeof(float));
        void* bufferData = glMapBuffer(GL_ARRAY_BUFFER, GL_READ_ONLY);
        if (bufferData) {
            std::memcpy(meshData.VertexData.data(), bufferData, bufferSize);
            glUnmapBuffer(GL_ARRAY_BUFFER);
        } else {
            HE_CORE_ERROR("MeshData::FromVertexArray - Failed to map vertex buffer");
            return {};
        }

        // 获取索引缓冲区数据
        const auto& indexBuffer = vertexArray->GetIndexBuffer();
        if (!indexBuffer) {
            HE_CORE_WARN("MeshData::FromVertexArray - No index buffer found");
            return {};
        }

        auto openglIndexBuffer = std::dynamic_pointer_cast<OpenGLIndexBuffer>(indexBuffer);
        if (!openglIndexBuffer) {
            HE_CORE_WARN("MeshData::FromVertexArray - Failed to cast to OpenGLIndexBuffer");
            return {};
        }

        // 绑定并读取索引数据
        openglIndexBuffer->Bind();
        
        GLint indexBufferSize;
        glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexBufferSize);
        
        if (indexBufferSize <= 0) {
            HE_CORE_WARN("MeshData::FromVertexArray - Invalid index buffer size: {}", indexBufferSize);
            return {};
        }

        meshData.IndexData.resize(indexBufferSize / sizeof(uint32_t));
        void* indexBufferData = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_READ_ONLY);
        if (indexBufferData) {
            std::memcpy(meshData.IndexData.data(), indexBufferData, indexBufferSize);
            glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
        } else {
            HE_CORE_ERROR("MeshData::FromVertexArray - Failed to map index buffer");
            return {};
        }

        HE_CORE_INFO("MeshData::FromVertexArray - Successfully extracted {} vertices, {} indices", 
                     meshData.VertexData.size(), meshData.IndexData.size());
        
        return meshData;
    }

    Ref<VertexArray> MeshData::ToVertexArray() const {
        if (!IsValid()) {
            HE_CORE_WARN("MeshData::ToVertexArray - Invalid mesh data");
            return nullptr;
        }

        // 创建顶点数组
        auto vertexArray = VertexArray::Create();

        // 创建顶点缓冲区
        auto vertexBuffer = VertexBuffer::Create(
            const_cast<float*>(VertexData.data()), 
            static_cast<uint32_t>(VertexData.size() * sizeof(float))
        );

        // 设置布局
        auto bufferLayout = Layout.ToBufferLayout();
        vertexBuffer->SetLayout(bufferLayout);
        vertexArray->AddVertexBuffer(vertexBuffer);

        // 创建索引缓冲区
        auto indexBuffer = IndexBuffer::Create(
            const_cast<uint32_t*>(IndexData.data()), 
            static_cast<uint32_t>(IndexData.size())
        );
        vertexArray->SetIndexBuffer(indexBuffer);

        HE_CORE_INFO("MeshData::ToVertexArray - Successfully created VertexArray with {} vertices, {} indices", 
                     VertexData.size(), IndexData.size());

        return vertexArray;
    }

    BufferLayout SerializableBufferLayout::ToBufferLayout() const {
        if (Elements.empty()) {
            return BufferLayout{};
        }

        // 创建 BufferElement 向量
        std::vector<BufferElement> bufferElements;
        bufferElements.reserve(Elements.size());
        for (const auto& element : Elements) {
            bufferElements.push_back(element.ToBufferElement());
        }

        // 使用反射访问 BufferLayout 的私有成员来构造
        BufferLayout layout;
        
        // 我们需要通过友元函数或其他方法来设置内部数据
        // 暂时使用初始化列表，支持最常见的情况
        switch (bufferElements.size()) {
            case 0: return BufferLayout{};
            case 1: return BufferLayout{bufferElements[0]};
            case 2: return BufferLayout{bufferElements[0], bufferElements[1]};
            case 3: return BufferLayout{bufferElements[0], bufferElements[1], bufferElements[2]};
            case 4: return BufferLayout{bufferElements[0], bufferElements[1], bufferElements[2], bufferElements[3]};
            case 5: return BufferLayout{bufferElements[0], bufferElements[1], bufferElements[2], bufferElements[3], bufferElements[4]};
            default:
                HE_CORE_WARN("ToBufferLayout: Too many elements ({}), only first 5 supported", bufferElements.size());
                return BufferLayout{bufferElements[0], bufferElements[1], bufferElements[2], bufferElements[3], bufferElements[4]};
        }
    }
}
