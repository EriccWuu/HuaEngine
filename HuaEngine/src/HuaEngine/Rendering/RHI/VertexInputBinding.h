#pragma once

#include <cstdint>

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Rendering/RHI/GpuBuffer.h"
#include "HuaEngine/Rendering/RHI/VertexBufferView.h"

namespace HE::Rendering {
	struct VertexBufferBinding {
		Ref<GpuBuffer> Buffer;
		uint32_t Offset = 0;
		uint32_t Stride = 0;
	};

	struct IndexBufferBinding {
		Ref<GpuBuffer> Buffer;
		uint32_t Offset = 0;
		IndexFormat Format = IndexFormat::UInt32;
		uint32_t IndexCount = 0;
	};
}
