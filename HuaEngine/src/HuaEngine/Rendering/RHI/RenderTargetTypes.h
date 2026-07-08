#pragma once

#include <cstdint>
#include <initializer_list>
#include <vector>

namespace HE::Rendering {
	enum class RenderTargetTextureFormat {
		None = 0,
		RGBA8,
		RED_INTEGER,
		DEPTH24_STENCIL8,
		DEPTH = DEPTH24_STENCIL8
	};

	struct RenderTargetTextureSpecification {
		RenderTargetTextureSpecification() = default;
		RenderTargetTextureSpecification(RenderTargetTextureFormat format)
			: Format(format) {}

		RenderTargetTextureFormat Format = RenderTargetTextureFormat::None;
	};

	struct RenderTargetAttachmentSpecification {
		RenderTargetAttachmentSpecification() = default;
		RenderTargetAttachmentSpecification(std::initializer_list<RenderTargetTextureSpecification> attachments)
			: Attachments(attachments) {}

		std::vector<RenderTargetTextureSpecification> Attachments;
	};

	struct RenderTargetSpecification {
		uint32_t Width = 0;
		uint32_t Height = 0;
		uint32_t Samples = 1;
		RenderTargetAttachmentSpecification Attachments;
		bool SwapChainTarget = false;
	};

	struct RenderTargetPixelRGBA8 {
		uint8_t R = 0;
		uint8_t G = 0;
		uint8_t B = 0;
		uint8_t A = 0;
	};
}
