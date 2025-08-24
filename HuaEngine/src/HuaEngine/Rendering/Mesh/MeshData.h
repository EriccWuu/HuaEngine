#pragma once

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Rendering/VertexArray.h"
#include "HuaEngine/Rendering/VertexBuffer.h"
#include "HuaEngine/Reflection/Reflection.h"
#include <vector>

namespace HE {
    // 可序列化的缓冲区布局元素
    struct SerializableBufferElement {
        SerializableBufferElement() = default;
        SerializableBufferElement(const BufferElement& element)
            : Type(static_cast<uint8_t>(element.Type))
            , Name(element.Name)
            , Size(element.Size)
            , Offset(element.Offset)
            , Normalized(element.Normalized) {}
        
        // 直接构造函数
        SerializableBufferElement(uint8_t type, const std::string& name, uint32_t size, uint32_t offset, bool normalized)
            : Type(type), Name(name), Size(size), Offset(offset), Normalized(normalized) {}

        uint8_t Type;           // ShaderDataType as uint8_t
        std::string Name;
        uint32_t Size;
        uint32_t Offset;
        bool Normalized;

        // 转换回 BufferElement
        BufferElement ToBufferElement() const {
            BufferElement element(static_cast<ShaderDataType>(Type), Name, Normalized);
            element.Size = Size;
            element.Offset = Offset;
            return element;
        }
    };

    // 可序列化的缓冲区布局
    struct SerializableBufferLayout {
        SerializableBufferLayout() = default;
        SerializableBufferLayout(const BufferLayout& layout) : Stride(layout.GetStride()) {
            const auto& elements = layout.GetElements();
            Elements.reserve(elements.size());
            for (const auto& element : elements) {
                Elements.emplace_back(element);
            }
        }

        std::vector<SerializableBufferElement> Elements;
        uint32_t Stride;

        // 转换回 BufferLayout (使用 BufferLayout 的扩展构造函数)
        BufferLayout ToBufferLayout() const;  // 声明，在 cpp 中实现
    };

    // 可序列化的网格数据
    struct MeshData {
        std::vector<float> VertexData;              // 顶点数据（展平的浮点数组）
        std::vector<uint32_t> IndexData;            // 索引数据
        SerializableBufferLayout Layout;            // 顶点布局
        
        // 从 VertexArray 提取数据
        static MeshData FromVertexArray(const Ref<VertexArray>& vertexArray);
        
        // 创建 VertexArray
        Ref<VertexArray> ToVertexArray() const;
        
        // 验证数据有效性
        bool IsValid() const {
            return !VertexData.empty() && !IndexData.empty() && !Layout.Elements.empty();
        }
    };
}

// 反射注册
srefl_class(HE::SerializableBufferElement,
    fields(
        field(Type),
        field(Name),
        field(Size),
        field(Offset),
        field(Normalized)
    )
)

srefl_class(HE::SerializableBufferLayout,
    fields(
        field(Elements),
        field(Stride)
    )
)

srefl_class(HE::MeshData,
    fields(
        field(VertexData),
        field(IndexData),
        field(Layout)
    )
)
