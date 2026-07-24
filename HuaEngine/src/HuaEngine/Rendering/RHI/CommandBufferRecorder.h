#pragma once

#include "HuaEngine/Rendering/RHI/CommandList.h"
#include "HuaEngine/Rendering/RHI/CommandSubmission.h"

namespace HE::Rendering {
	class CommandBufferRecorder final : public CommandList {
	public:
		explicit CommandBufferRecorder(CommandBuffer& commandBuffer)
			: m_CommandBuffer(commandBuffer) {}

		[[nodiscard]] bool Succeeded() const { return m_Succeeded; }

		void BeginRenderPass(const RenderPassDesc& desc) override;
		void EndRenderPass() override;
		void ResourceBarrier(const HE::Rendering::ResourceBarrier& barrier) override;
		void BeginRenderTarget(RenderTarget& target) override;
		void ClearColor(const glm::vec4& color) override;
		void BeginFrame() override;
		void SetPipelineState(PipelineState& pipelineState) override;
		void SetVertexBuffer(uint32_t slot, const VertexBufferBinding& binding) override;
		void SetIndexBuffer(const IndexBufferBinding& binding) override;
		void SetVertexBufferView(VertexBufferView& vertexBufferView) override;
		void SetBindGroup(uint32_t slot, BindGroup& bindGroup) override;
		void DrawIndexed(uint32_t indexCount) override;
		void EndFrame() override;
		void EndRenderTarget() override;

	private:
		void MarkUnsupported();

		CommandBuffer& m_CommandBuffer;
		bool m_Succeeded = true;
	};
}
