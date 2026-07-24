#include "enginepch.h"
#include "CommandBufferRecorder.h"

namespace HE::Rendering {
	void CommandBufferRecorder::BeginRenderPass(const RenderPassDesc& desc) {
		m_Succeeded = m_CommandBuffer.RecordBeginRenderPass(desc) && m_Succeeded;
	}

	void CommandBufferRecorder::EndRenderPass() {
		m_Succeeded = m_CommandBuffer.RecordEndRenderPass() && m_Succeeded;
	}

	void CommandBufferRecorder::ResourceBarrier(const HE::Rendering::ResourceBarrier& barrier) {
		m_Succeeded = m_CommandBuffer.RecordResourceBarrier(barrier) && m_Succeeded;
	}

	void CommandBufferRecorder::BeginRenderTarget(RenderTarget&) {
		MarkUnsupported();
	}

	void CommandBufferRecorder::ClearColor(const glm::vec4&) {
		MarkUnsupported();
	}

	void CommandBufferRecorder::BeginFrame() {
		m_Succeeded = m_CommandBuffer.RecordBeginFrame() && m_Succeeded;
	}

	void CommandBufferRecorder::SetPipelineState(PipelineState& pipelineState) {
		m_Succeeded = m_CommandBuffer.RecordSetPipelineState(pipelineState) && m_Succeeded;
	}

	void CommandBufferRecorder::SetVertexBuffer(uint32_t slot, const VertexBufferBinding& binding) {
		m_Succeeded = m_CommandBuffer.RecordSetVertexBuffer(slot, binding) && m_Succeeded;
	}

	void CommandBufferRecorder::SetIndexBuffer(const IndexBufferBinding& binding) {
		m_Succeeded = m_CommandBuffer.RecordSetIndexBuffer(binding) && m_Succeeded;
	}

	void CommandBufferRecorder::SetVertexBufferView(VertexBufferView&) {
		MarkUnsupported();
	}

	void CommandBufferRecorder::SetBindGroup(uint32_t slot, BindGroup& bindGroup) {
		m_Succeeded = m_CommandBuffer.RecordSetBindGroup(slot, bindGroup) && m_Succeeded;
	}

	void CommandBufferRecorder::DrawIndexed(uint32_t indexCount) {
		m_Succeeded = m_CommandBuffer.RecordDrawIndexed(indexCount) && m_Succeeded;
	}

	void CommandBufferRecorder::EndFrame() {
		m_Succeeded = m_CommandBuffer.RecordEndFrame() && m_Succeeded;
	}

	void CommandBufferRecorder::EndRenderTarget() {
		MarkUnsupported();
	}

	void CommandBufferRecorder::MarkUnsupported() {
		m_Succeeded = false;
	}
}
