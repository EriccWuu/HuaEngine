#pragma once

#include <cstdint>
#include <string>

namespace HE::Rendering {
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
	};

	class RenderQueue {
	public:
		virtual ~RenderQueue() = default;

		virtual void Submit(CommandBuffer& commandBuffer) = 0;
	};
}
