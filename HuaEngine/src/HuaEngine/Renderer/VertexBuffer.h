#pragma once

#include "Buffer.h"
#include <cstdint>

namespace HE {
	enum class ShaderDataType : uint8_t {
		None = 0,
		Float, Float2, Float3, Float4,
		Int, Int2, Int3, Int4,
		Mat3, Mat4,
		Bool
	};

	static uint32_t ShaderDataTypeSize(ShaderDataType dataType) {
		switch (dataType) {
			case ShaderDataType::Float:  return 4;
			case ShaderDataType::Float2: return 4 * 2;
			case ShaderDataType::Float3: return 4 * 3;
			case ShaderDataType::Float4: return 4 * 4;
			case ShaderDataType::Int:    return 4;
			case ShaderDataType::Int2:   return 4 * 2;
			case ShaderDataType::Int3:   return 4 * 3;
			case ShaderDataType::Int4:   return 4 * 4;
			case ShaderDataType::Mat3:   return 4 * 3 * 3;
			case ShaderDataType::Mat4:   return 4 * 4 * 4;
			case ShaderDataType::Bool:   return 1;
		}
		HE_CORE_ASSERT(false, "Unkonwn ShaderDataType");
		return 0;
	}

	static uint32_t ShaderDataTypeByteCount(ShaderDataType dataType) {
		switch (dataType) {
		case ShaderDataType::Float:  return 1;
		case ShaderDataType::Float2: return 2;
		case ShaderDataType::Float3: return 3;
		case ShaderDataType::Float4: return 4;
		case ShaderDataType::Int:    return 1;
		case ShaderDataType::Int2:   return 2;
		case ShaderDataType::Int3:   return 3;
		case ShaderDataType::Int4:   return 4;
		case ShaderDataType::Mat3:   return 3 * 3;
		case ShaderDataType::Mat4:   return 4 * 4;
		case ShaderDataType::Bool:   return 1;
		}
		HE_CORE_ASSERT(false, "Unkonwn ShaderDataType");
		return 0;
	}

	struct BufferElement {
		BufferElement(ShaderDataType type, std::string name, bool normalized = false)
			: Type(type), Name(name), Normalized(normalized), Offset(0), Size(ShaderDataTypeSize(type)) {

		}

		ShaderDataType Type;
		std::string Name;
		uint32_t Size;
		uint32_t Offset;
		bool Normalized;
	};

	class BufferLayout {
	public:
		BufferLayout() = default;
		BufferLayout(std::initializer_list<BufferElement> bufferElemets)
			: m_Elemets(bufferElemets), m_Stride(0) {
			CalcBufferLayout();
		}

		uint32_t inline GetStride() const { return m_Stride; }
		inline const std::vector<BufferElement>& GetElements() const { return m_Elemets; }

		std::vector<BufferElement>::iterator begin() { return m_Elemets.begin(); }
		std::vector<BufferElement>::iterator end() { return m_Elemets.end(); }
		std::vector<BufferElement>::const_iterator begin() const { return m_Elemets.begin(); }
		std::vector<BufferElement>::const_iterator end() const { return m_Elemets.end(); }

	private:
		std::vector<BufferElement> m_Elemets;
		uint32_t m_Stride;

		void CalcBufferLayout() {
			uint32_t offset = 0;
			for (auto& element : m_Elemets) {
				element.Offset = offset;
				m_Stride += element.Size;
				offset += element.Size;
			}
		}
	};

	class VertexBuffer : public Buffer {
	public:
		virtual ~VertexBuffer() {}
		static Ref<VertexBuffer> Create(float* vertices, uint32_t size);
		virtual inline void SetLayout(BufferLayout& layout) = 0;
		virtual inline const BufferLayout& GetLayout() const = 0;
	};
}