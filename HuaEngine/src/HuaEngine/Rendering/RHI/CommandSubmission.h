#pragma once

#include <cstdint>
#include <string>

namespace HE::Rendering {
	class BindGroup;
	class CommandList;
	class PipelineState;
	struct IndexBufferBinding;
	struct RenderPassDesc;
	struct VertexBufferBinding;

	enum class CommandBufferUsage : uint8_t {
		Invalid = 0,
		Graphics
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
		virtual bool RecordSetPipelineState(PipelineState& pipelineState) = 0;
		virtual bool RecordSetVertexBuffer(uint32_t slot, const VertexBufferBinding& binding) = 0;
		virtual bool RecordSetIndexBuffer(const IndexBufferBinding& binding) = 0;
		virtual bool RecordSetBindGroup(uint32_t slot, BindGroup& bindGroup) = 0;
		virtual bool RecordDrawIndexed(uint32_t indexCount) = 0;
	};

	class RenderQueue {
	public:
		virtual ~RenderQueue() = default;

		virtual bool Submit(CommandBuffer& commandBuffer) = 0;
	};
}
