#pragma once

#include "HuaEngine/Core/Core.h"

namespace HE {
	enum class FrameBufferTextureFormat {
		None = 0,
		RGBA8,
		RED_INTEGER,
		DEPTH24_STENCIL8,
		DEPTH = DEPTH24_STENCIL8
	};

	struct FrameBufferTextureSpecification {
		FrameBufferTextureSpecification() = default;
		FrameBufferTextureSpecification(FrameBufferTextureFormat format)
			: Format(format) {}

		FrameBufferTextureFormat Format = FrameBufferTextureFormat::None;
	};

	struct FrameBufferAttachmentSpecification {
		FrameBufferAttachmentSpecification() = default;
		FrameBufferAttachmentSpecification(std::initializer_list<FrameBufferTextureSpecification> attachments)
			: Attachments(attachments) {}

		std::vector<FrameBufferTextureSpecification> Attachments;
	};

	struct FrameBufferSpecification {
		uint32_t Width = 0;
		uint32_t Height = 0;
		uint32_t Samples = 1;
		FrameBufferAttachmentSpecification Attachments;
		bool SwapChainTarget = false;
	};

	class FrameBuffer {
	public:
		virtual ~FrameBuffer() = default;

		virtual void Bind() = 0;
		virtual void Unbind() = 0;

		virtual void Resize(uint32_t width, uint32_t height) = 0;

		virtual uint32_t GetRenderID() const = 0;

		virtual void ClearAttachment(uint32_t index, int value) = 0;
		virtual uint32_t GetColorAttachment(uint32_t index = 0) const = 0;

		virtual const FrameBufferSpecification& GetSpecification() const = 0;

		static Ref<FrameBuffer> Create(const FrameBufferSpecification& specification);
	};
}