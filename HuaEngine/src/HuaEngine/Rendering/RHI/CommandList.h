#pragma once

#include <cstdint>
#include "glm/glm.hpp"
#include "HuaEngine/Rendering/RHI/RenderPass.h"
#include "HuaEngine/Rendering/RHI/ResourceBarrier.h"
#include "HuaEngine/Rendering/RHI/VertexInputBinding.h"

namespace HE::Rendering {
	class BindGroup;
	class PipelineState;
	class RenderTarget;
	class VertexBufferView;

	class CommandList {
	public:
		virtual ~CommandList() = default;

		virtual void BeginRenderPass(const RenderPassDesc& desc) = 0;
		virtual void EndRenderPass() = 0;
		virtual void ResourceBarrier(const HE::Rendering::ResourceBarrier& barrier) = 0;

		virtual void BeginRenderTarget(RenderTarget& target) = 0;
		virtual void ClearColor(const glm::vec4& color) = 0;
		virtual void BeginFrame() = 0;

		// Normal render path pipeline state submission.
		virtual void SetPipelineState(PipelineState& pipelineState) = 0;
		// Explicit vertex buffer binding submission.
		virtual void SetVertexBuffer(uint32_t slot, const VertexBufferBinding& binding) = 0;
		// Explicit index buffer binding submission.
		virtual void SetIndexBuffer(const IndexBufferBinding& binding) = 0;
		// Normal render path state submission.
		virtual void SetVertexBufferView(VertexBufferView& vertexBufferView) = 0;
		// Normal render path resource binding submission.
		virtual void SetBindGroup(uint32_t slot, BindGroup& bindGroup) = 0;
		// Normal render path draw. Pipeline-declared bind group slots must be submitted first.
		virtual void DrawIndexed(uint32_t indexCount) = 0;

		virtual void EndFrame() = 0;
		virtual void EndRenderTarget() = 0;
	};
}
