#pragma once

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Rendering/RHI/RenderTargetTypes.h"

namespace HE::Rendering {
	class TextureResource;
	class TextureView;
	struct RenderTargetDesc {
		RenderTargetSpecification Specification;
	};

	struct RenderTargetColorAttachmentView {
		uintptr_t NativeHandle = 0;
		RenderTargetTextureFormat Format = RenderTargetTextureFormat::None;
		uint32_t Width = 0;
		uint32_t Height = 0;
		uint32_t Samples = 1;
		uint32_t AttachmentIndex = 0;
	};

	class RenderTarget {
	public:
		virtual ~RenderTarget() = default;

		virtual const RenderTargetDesc& GetDesc() const = 0;
		virtual void Resize(uint32_t width, uint32_t height) = 0;
		virtual void ClearAttachment(uint32_t index, int value) = 0;
		virtual RenderTargetPixelRGBA8 ReadPixelRGBA8(uint32_t attachmentIndex, uint32_t x, uint32_t y) const = 0;
		virtual Ref<TextureResource> GetColorAttachmentTexture(uint32_t index = 0) const = 0;
		virtual Ref<TextureResource> GetDepthStencilAttachmentTexture() const = 0;
		virtual Ref<TextureView> GetColorAttachmentTextureView(uint32_t index = 0) const = 0;
		virtual Ref<TextureView> GetDepthStencilAttachmentTextureView() const = 0;
		virtual RenderTargetColorAttachmentView GetColorAttachmentView(uint32_t index = 0) const = 0;
		virtual RenderTargetColorAttachmentView GetDepthStencilAttachmentView() const = 0;
		virtual const RenderTargetSpecification& GetSpecification() const = 0;
	};
}
