#include "enginepch.h"
#include "UniformBufferArena.h"

#include <cstring>

#include "HuaEngine/Rendering/RHI/GpuBuffer.h"
#include "HuaEngine/Rendering/RHI/RenderDevice.h"

namespace HE::Rendering {
	UniformBufferArena::UniformBufferArena(RenderDevice& device, uint32_t capacity, uint32_t alignment)
		: m_Device(&device), m_Capacity(capacity), m_Alignment(std::max(1u, alignment == 0 ? device.GetCapabilities().UniformBufferOffsetAlignment : alignment)) {
		if (m_Capacity > 0) m_Buffer = device.CreateBuffer({ .Usage = GpuBufferUsage::Uniform, .Size = m_Capacity }, nullptr);
	}

	bool UniformBufferArena::Allocate(const void* data, uint32_t size, UniformBufferAllocation& output) {
		output = {};
		if (!m_Device || !m_Buffer || !data || size == 0) return false;
		const uint32_t alignedOffset = (m_Offset + m_Alignment - 1) / m_Alignment * m_Alignment;
		if (alignedOffset > m_Capacity || size > m_Capacity - alignedOffset) return false;
		std::vector<uint8_t> bytes(size);
		std::memcpy(bytes.data(), data, size);
		if (!m_Device->UploadBuffer({ .Buffer = m_Buffer, .Offset = alignedOffset, .Data = std::move(bytes) })) return false;
		output = { m_Buffer, alignedOffset, size };
		m_Offset = alignedOffset + size;
		return true;
	}

	void UniformBufferArena::BeginFrame(uint64_t completedFenceValue) {
		if (m_LastSignalFenceValue != 0 && completedFenceValue >= m_LastSignalFenceValue) {
			m_Offset = 0;
			m_LastSignalFenceValue = 0;
		}
	}

	void UniformBufferArena::SealFrame(uint64_t signalFenceValue) {
		m_LastSignalFenceValue = std::max(m_LastSignalFenceValue, signalFenceValue);
	}
}
