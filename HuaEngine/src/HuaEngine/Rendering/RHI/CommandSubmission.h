#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace HE::Rendering {
	class BindGroup;
	class CommandList;
	class PipelineState;
	struct ResourceBarrier;
	struct IndexBufferBinding;
	struct RenderPassDesc;
	struct VertexBufferBinding;

	enum class CommandBufferUsage : uint8_t {
		Invalid = 0,
		Graphics,
		Compute,
		Copy
	};

	enum class RenderQueueType : uint8_t {
		Graphics = 0,
		Compute,
		Copy
	};

	struct CommandBufferDesc {
		CommandBufferUsage Usage = CommandBufferUsage::Graphics;
		std::string DebugName;
	};

	class CommandBuffer {
	public:
		virtual ~CommandBuffer() = default;

		virtual const CommandBufferDesc& GetDesc() const = 0;
		virtual bool Begin() = 0;
		virtual bool End() = 0;
		virtual void Reset() = 0;
		virtual bool IsRecording() const = 0;
		virtual bool IsExecutable() const = 0;

		virtual bool RecordBeginRenderPass(const RenderPassDesc& desc) = 0;
		virtual bool RecordEndRenderPass() = 0;
		virtual bool RecordResourceBarrier(const ResourceBarrier& barrier) = 0;
		virtual bool RecordBeginFrame() = 0;
		virtual bool RecordSetPipelineState(PipelineState& pipelineState) = 0;
		virtual bool RecordSetVertexBuffer(uint32_t slot, const VertexBufferBinding& binding) = 0;
		virtual bool RecordSetIndexBuffer(const IndexBufferBinding& binding) = 0;
		virtual bool RecordSetBindGroup(uint32_t slot, BindGroup& bindGroup) = 0;
		virtual bool RecordDrawIndexed(uint32_t indexCount) = 0;
		virtual bool RecordEndFrame() = 0;
		virtual void RetainResource(const std::shared_ptr<void>& resource) = 0;
	};

	class Fence {
	public:
		virtual ~Fence() = default;

		virtual uint64_t GetCompletedValue() const = 0;
	};

	struct QueueSubmitDesc {
		CommandBuffer* CommandBufferPtr = nullptr;
		Fence* WaitFence = nullptr;
		uint64_t WaitValue = 0;
	};

	struct QueueSubmitResult {
		bool Succeeded = false;
		uint64_t SignalValue = 0;
		Fence* SignalFence = nullptr;

		operator bool() const { return Succeeded; }
	};

	class RenderQueue {
	public:
		virtual ~RenderQueue() = default;

		virtual QueueSubmitResult Submit(CommandBuffer& commandBuffer) = 0;
		virtual QueueSubmitResult Submit(const QueueSubmitDesc& desc) {
			if (!desc.CommandBufferPtr) {
				return {};
			}

			return Submit(*desc.CommandBufferPtr);
		}

		virtual Fence& GetTimelineFence() = 0;
		virtual RenderQueueType GetType() const = 0;
	};

	class DeferredReleaseQueue {
	public:
		void Track(std::shared_ptr<void> resource, Fence* fence, uint64_t value) {
			if (resource && fence) {
				m_Pending.push_back({ .Resource = std::move(resource), .FencePtr = fence, .Value = value });
			}
		}

		void RetireCompleted() {
			std::erase_if(m_Pending, [](const auto& entry) {
				return entry.FencePtr->GetCompletedValue() >= entry.Value;
			});
		}

		[[nodiscard]] uint32_t GetPendingCount() const { return static_cast<uint32_t>(m_Pending.size()); }

	private:
		struct Entry {
			std::shared_ptr<void> Resource;
			Fence* FencePtr = nullptr;
			uint64_t Value = 0;
		};

		std::vector<Entry> m_Pending;
	};
}
