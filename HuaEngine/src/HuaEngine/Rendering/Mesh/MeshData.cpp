#include "enginepch.h"
#include "MeshData.h"

#include "HuaEngine/Rendering/RHI/RenderHardwareInterface.h"

namespace HE::Rendering {
    Ref<VertexBufferView> MeshData::ToVertexBufferView() const {
        if (!IsValid()) {
            HE_CORE_WARN("MeshData::ToVertexBufferView - Invalid mesh data");
            return nullptr;
        }

        auto& device = RenderHardwareInterface::GetDevice();
        const auto bufferLayout = Layout.ToBufferLayout();

        auto vertexBuffer = device.CreateBuffer({
            .Usage = GpuBufferUsage::Vertex,
            .Size = static_cast<uint32_t>(VertexData.size() * sizeof(float)),
            .Stride = bufferLayout.GetStride()
        }, VertexData.data());

        if (!vertexBuffer) {
            HE_CORE_ERROR("MeshData::ToVertexBufferView - Failed to create vertex buffer");
            return nullptr;
        }

        auto indexBuffer = device.CreateBuffer({
            .Usage = GpuBufferUsage::Index,
            .Size = static_cast<uint32_t>(IndexData.size() * sizeof(uint32_t)),
            .Stride = static_cast<uint32_t>(sizeof(uint32_t))
        }, IndexData.data());

        if (!indexBuffer) {
            HE_CORE_ERROR("MeshData::ToVertexBufferView - Failed to create index buffer");
            return nullptr;
        }

        auto vertexBufferView = device.CreateVertexBufferView({
            .VertexBuffer = vertexBuffer,
            .IndexBuffer = indexBuffer,
            .Layout = bufferLayout,
            .IndexFormatValue = IndexFormat::UInt32,
            .IndexCount = static_cast<uint32_t>(IndexData.size())
        });

        if (!vertexBufferView) {
            HE_CORE_ERROR("MeshData::ToVertexBufferView - Failed to create vertex buffer view");
            return nullptr;
        }

        HE_CORE_INFO("MeshData::ToVertexBufferView - Successfully created VertexBufferView with {} vertices, {} indices",
                     VertexData.size(), IndexData.size());

        return vertexBufferView;
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
