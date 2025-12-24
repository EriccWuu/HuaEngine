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

        // Get vertex buffer data
        const auto& vertexBuffers = vertexArray->GetVertexBuffers();
        if (vertexBuffers.empty()) {
            HE_CORE_WARN("MeshData::FromVertexArray - No vertex buffers found");
            return {};
        }

        // Currently only supports single vertex buffer
        const auto& vertexBuffer = vertexBuffers[0];
        if (!vertexBuffer) {
            HE_CORE_WARN("MeshData::FromVertexArray - First vertex buffer is null");
            return {};
        }

        // Save layout information
        meshData.Layout = SerializableBufferLayout(vertexBuffer->GetLayout());

        // Read vertex data from OpenGL buffer
        auto openglBuffer = std::dynamic_pointer_cast<OpenGLVertexBuffer>(vertexBuffer);
        if (!openglBuffer) {
            HE_CORE_WARN("MeshData::FromVertexArray - Failed to cast to OpenGLVertexBuffer");
            return {};
        }

        // Bind and read vertex data
        openglBuffer->Bind();

        // Get buffer size
        GLint bufferSize;
        glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &bufferSize);
        
        if (bufferSize <= 0) {
            HE_CORE_WARN("MeshData::FromVertexArray - Invalid buffer size: {}", bufferSize);
            return {};
        }

        // Allocate memory and read data
        meshData.VertexData.resize(bufferSize / sizeof(float));
        void* bufferData = glMapBuffer(GL_ARRAY_BUFFER, GL_READ_ONLY);
        if (bufferData) {
            std::memcpy(meshData.VertexData.data(), bufferData, bufferSize);
            glUnmapBuffer(GL_ARRAY_BUFFER);
        } else {
            HE_CORE_ERROR("MeshData::FromVertexArray - Failed to map vertex buffer");
            return {};
        }

        // Get index buffer data
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

        // Bind and read index data
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

        // Create vertex array
        auto vertexArray = VertexArray::Create();

        // Create vertex buffer
        auto vertexBuffer = VertexBuffer::Create(
            const_cast<float*>(VertexData.data()), 
            static_cast<uint32_t>(VertexData.size() * sizeof(float))
        );

        // Set layout
        auto bufferLayout = Layout.ToBufferLayout();
        vertexBuffer->SetLayout(bufferLayout);
        vertexArray->AddVertexBuffer(vertexBuffer);

        // Create index buffer
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

        // Create BufferElement vector
        std::vector<BufferElement> bufferElements;
        bufferElements.reserve(Elements.size());
        for (const auto& element : Elements) {
            bufferElements.push_back(element.ToBufferElement());
        }

        // Construct BufferLayout
        // Use initializer list for most common cases
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
