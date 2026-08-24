#pragma once

#include <cstdint>
#include <vector>

#include "HuaEngine/Core/Core.h"

namespace HE::Rendering {
	class GpuBuffer;
	class RenderDevice;

	struct UniformBufferAllocation {
		Ref<GpuBuffer> Buffer;
		uint32_t Offset = 0;
		uint32_t Size = 0;
	};

	class UniformBufferArena final {
	public:
		explicit UniformBufferArena(RenderDevice& device, uint32_t capacity = 4 * 1024 * 1024, uint32_t alignment = 0);

		bool Allocate(const void* data, uint32_t size, UniformBufferAllocation& output);
		void BeginFrame(uint64_t completedFenceValue);
		void SealFrame(uint64_t signalFenceValue);
		uint32_t GetCapacity() const { return m_Capacity; }
		uint32_t GetUsedSize() const { return m_Offset; }
		uint32_t GetBackingBufferCount() const { return m_Buffer ? 1u : 0u; }

	private:
		RenderDevice* m_Device = nullptr;
		Ref<GpuBuffer> m_Buffer;
		uint32_t m_Capacity = 0;
		uint32_t m_Alignment = 1;
		uint32_t m_Offset = 0;
		uint64_t m_LastSignalFenceValue = 0;
	};
}
