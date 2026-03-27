#pragma once

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Rendering/VertexArray.h"
#include "HuaEngine/Rendering/VertexBuffer.h"
#include "HuaEngine/Serialization/SerializationCore.h"
#include <vector>

namespace HE::Rendering {
    // Serializable buffer layout element
    struct SerializableBufferElement {
        SerializableBufferElement() = default;
        SerializableBufferElement(const BufferElement& element)
            : Type(static_cast<uint8_t>(element.Type))
            , Name(element.Name)
            , Size(element.Size)
            , Offset(element.Offset)
            , Normalized(element.Normalized) {}

        // Direct constructor
        SerializableBufferElement(uint8_t type, const std::string& name, uint32_t size, uint32_t offset, bool normalized)
            : Type(type), Name(name), Size(size), Offset(offset), Normalized(normalized) {}

        uint8_t Type;           // ShaderDataType as uint8_t
        std::string Name;
        uint32_t Size;
        uint32_t Offset;
        bool Normalized;

        // Convert back to BufferElement
        BufferElement ToBufferElement() const {
            BufferElement element(static_cast<ShaderDataType>(Type), Name, Normalized);
            element.Size = Size;
            element.Offset = Offset;
            return element;
        }
    };

    // Serializable buffer layout
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

        // Convert back to BufferLayout (using BufferLayout's extended constructor)
        BufferLayout ToBufferLayout() const;  // Declaration, implemented in cpp
    };

    // Serializable mesh data
    struct MeshData {
        std::vector<float> VertexData;              // Vertex data (flattened float array)
        std::vector<uint32_t> IndexData;            // Index data
        SerializableBufferLayout Layout;            // Vertex layout

        // Extract data from VertexArray
        static MeshData FromVertexArray(const Ref<VertexArray>& vertexArray);

        // Create VertexArray
        Ref<VertexArray> ToVertexArray() const;

        // Validate data
        bool IsValid() const {
            return !VertexData.empty() && !IndexData.empty() && !Layout.Elements.empty();
        }
    };
}

// Reflection registration
srefl_class(HE::Rendering::SerializableBufferElement,
    fields(
        field(Type),
        field(Name),
        field(Size),
        field(Offset),
        field(Normalized)
    )
)

srefl_class(HE::Rendering::SerializableBufferLayout,
    fields(
        field(Elements),
        field(Stride)
    )
)

srefl_class(HE::Rendering::MeshData,
    fields(
        field(VertexData),
        field(IndexData),
        field(Layout)
    )
)

namespace HE::Serialization {
    template<>
    struct Serializer<HE::Rendering::SerializableBufferElement> {
        static void Serialize(SerializationBackend& backend, const std::string& name, const HE::Rendering::SerializableBufferElement& element) {
            backend.BeginObject(name);
            backend.Serialize("shader_data_type", element.Type);
            backend.Serialize("attribute_name", element.Name);
            backend.Serialize("size", element.Size);
            backend.Serialize("offset", element.Offset);
            backend.Serialize("normalized", element.Normalized);
            backend.EndObject();
        }

        static bool Deserialize(SerializationBackend& backend, const std::string& name, HE::Rendering::SerializableBufferElement& element) {
            if (!name.empty() && !backend.HasField(name)) {
                return false;
            }

            backend.BeginObject(name);

            bool success = true;
            success &= DeserializeValue(backend, "shader_data_type", element.Type);
            success &= DeserializeValue(backend, "attribute_name", element.Name);
            success &= DeserializeValue(backend, "size", element.Size);
            success &= DeserializeValue(backend, "offset", element.Offset);
            success &= DeserializeValue(backend, "normalized", element.Normalized);

            backend.EndObject();
            return success;
        }
    };

    template<>
    struct Serializer<HE::Rendering::SerializableBufferLayout> {
        static void Serialize(SerializationBackend& backend, const std::string& name, const HE::Rendering::SerializableBufferLayout& layout) {
            backend.BeginObject(name);
            SerializeValue(backend, "elements", layout.Elements);
            backend.Serialize("stride", layout.Stride);
            backend.EndObject();
        }

        static bool Deserialize(SerializationBackend& backend, const std::string& name, HE::Rendering::SerializableBufferLayout& layout) {
            if (!name.empty() && !backend.HasField(name)) {
                return false;
            }

            backend.BeginObject(name);

            bool success = true;
            success &= DeserializeValue(backend, "elements", layout.Elements);
            success &= DeserializeValue(backend, "stride", layout.Stride);

            backend.EndObject();
            return success;
        }
    };

    template<>
    struct Serializer<HE::Rendering::MeshData> {
        static void Serialize(SerializationBackend& backend, const std::string& name, const HE::Rendering::MeshData& meshData) {
            backend.BeginObject(name);
            SerializeValue(backend, "vertex_data", meshData.VertexData);
            SerializeValue(backend, "index_data", meshData.IndexData);
            SerializeValue(backend, "layout", meshData.Layout);
            backend.EndObject();
        }

        static bool Deserialize(SerializationBackend& backend, const std::string& name, HE::Rendering::MeshData& meshData) {
            if (!name.empty() && !backend.HasField(name)) {
                return false;
            }

            backend.BeginObject(name);

            bool success = true;
            success &= DeserializeValue(backend, "vertex_data", meshData.VertexData);
            success &= DeserializeValue(backend, "index_data", meshData.IndexData);
            success &= DeserializeValue(backend, "layout", meshData.Layout);

            backend.EndObject();
            return success;
        }
    };
}
