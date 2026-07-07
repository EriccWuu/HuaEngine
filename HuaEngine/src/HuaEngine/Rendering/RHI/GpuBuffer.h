#pragma once

#include <cstdint>

namespace HE::Rendering {
	enum class GpuBufferUsage : uint8_t {
		Vertex = 0,
		Index,
		Uniform,
		Storage
	};

	struct GpuBufferDesc {
		GpuBufferUsage Usage = GpuBufferUsage::Vertex;
		uint32_t Size = 0;
		uint32_t Stride = 0;
	};

	class GpuBuffer {
	public:
		virtual ~GpuBuffer() = default;

		virtual const GpuBufferDesc& GetDesc() const = 0;
	};
}
