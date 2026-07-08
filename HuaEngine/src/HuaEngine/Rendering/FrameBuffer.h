#pragma once

#include "HuaEngine/Core/Core.h"

namespace HE::Rendering {
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

	struct FrameBufferPixelRGBA8 {
		uint8_t R = 0;
		uint8_t G = 0;
		uint8_t B = 0;
		uint8_t A = 0;
	};
}
