#pragma once 

#include "HuaEngine/Rendering/RHI/RenderTargetTypes.h"

namespace HE::Rendering {
	class OpenGLRenderTargetStorage {
	public:
		OpenGLRenderTargetStorage(const RenderTargetSpecification& specificationn);
		~OpenGLRenderTargetStorage();

		void BeginForCommandList();
		void EndForCommandList();

		void Resize(uint32_t width, uint32_t height);

		void ClearAttachment(uint32_t index, int value);
		RenderTargetPixelRGBA8 ReadPixelRGBA8(uint32_t attachmentIndex, uint32_t x, uint32_t y) const;

		uint32_t GetRenderID() const;

		uint32_t GetColorAttachment(uint32_t index = 0) const;
		uint32_t GetDepthAttachment() const;
		const RenderTargetSpecification& GetSpecification() const;

		void Invalidate();

	private:
		uint32_t m_RenderID = 0;
		RenderTargetSpecification m_Specification;

		std::vector<RenderTargetTextureSpecification> m_ColorAttachmentSpecifications;
		RenderTargetTextureSpecification m_DepthAttachmentSpecification = { RenderTargetTextureFormat::None };

		std::vector<uint32_t> m_ColorAttachments;
		uint32_t m_DepthAttachment = 0;
	};
}
