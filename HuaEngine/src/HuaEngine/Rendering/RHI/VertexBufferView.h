#pragma once

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Rendering/RHI/GpuBuffer.h"
#include "HuaEngine/Rendering/VertexBuffer.h"

namespace HE::Rendering {
	enum class IndexFormat : uint8_t {
		UInt32 = 0
	};

	struct VertexBufferViewDesc {
		Ref<GpuBuffer> VertexBuffer;
		Ref<GpuBuffer> IndexBuffer;
		BufferLayout Layout;
		IndexFormat IndexFormatValue = IndexFormat::UInt32;
		uint32_t IndexCount = 0;
	};

	class VertexBufferView {
	public:
		virtual ~VertexBufferView() = default;

		virtual const VertexBufferViewDesc& GetDesc() const = 0;
	};
}
