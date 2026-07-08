#pragma once

#include "HuaEngine/Rendering/RHI/RenderTargetTypes.h"

namespace HE::Rendering {
	struct RenderTargetDesc {
		RenderTargetSpecification Specification;
	};

	class RenderTarget {
	public:
		virtual ~RenderTarget() = default;

		virtual const RenderTargetDesc& GetDesc() const = 0;
		virtual void Resize(uint32_t width, uint32_t height) = 0;
		virtual void ClearAttachment(uint32_t index, int value) = 0;
		virtual RenderTargetPixelRGBA8 ReadPixelRGBA8(uint32_t attachmentIndex, uint32_t x, uint32_t y) const = 0;
		virtual uint32_t GetRenderID() const = 0;
		virtual uint32_t GetColorAttachment(uint32_t index = 0) const = 0;
		virtual const RenderTargetSpecification& GetSpecification() const = 0;
	};
}
