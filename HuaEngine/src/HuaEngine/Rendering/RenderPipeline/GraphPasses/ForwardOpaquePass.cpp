#include "enginepch.h"
#include "ForwardOpaquePass.h"

#include "HuaEngine/Rendering/RenderPipeline/RenderBindGroupBuilder.h"
#include "HuaEngine/Rendering/RenderPipeline/RenderResourceResolver.h"
#include "HuaEngine/Rendering/RHI/CommandBufferRecorder.h"
#include "HuaEngine/Rendering/RHI/CommandList.h"
#include "HuaEngine/Rendering/RHI/RenderHardwareInterface.h"

namespace HE::Rendering {
	void ForwardOpaquePass::Configure(
		RenderGraphResourceHandle sceneColor,
		RenderGraphResourceHandle sceneDepth,
		bool writeDepth,
		const glm::vec4& clearColor,
		bool clearColorBuffer) {
		m_SceneColor = sceneColor;
		m_SceneDepth = sceneDepth;
		m_WriteDepth = writeDepth;
		m_ClearColor = clearColor;
		m_ClearColorBuffer = clearColorBuffer;
	}

	void ForwardOpaquePass::Setup(RenderGraphPassBuilder& builder) {
		const auto load = m_ClearColorBuffer ? LoadOp::Clear : LoadOp::Load;
		builder.WriteColor(m_SceneColor, load, StoreOp::Store, m_ClearColor);
		if (m_WriteDepth) {
			builder.WriteDepth(m_SceneDepth, load);
		}
	}

	void ForwardOpaquePass::Execute(RenderPassContext& context) {
		if (!context.RenderItems || !context.ResourceResolver || !context.Commands || !context.Stats || !context.Diagnostics) {
			return;
		}

		++context.Stats->PassCount;

		if (!context.View || !context.View->CameraRef) {
			return;
		}

		auto& device = RenderHardwareInterface::GetDevice();
		auto& uniformArena = context.ResourceResolver->GetUniformBufferArena(device);
		for (const auto& item : *context.RenderItems) {
			ResolvedRenderItem resolvedItem;
			if (!context.ResourceResolver->Resolve(item, resolvedItem, *context.Stats, *context.Diagnostics)) {
				++context.Stats->SkippedItems;
				continue;
			}

			auto frameBindGroup = CreateFrameBindGroup(device, uniformArena, resolvedItem.FrameBlock, resolvedItem.FrameBindGroupLayoutRef, context.View->CameraRef->GetViewProjection());
			auto objectBindGroup = CreateObjectBindGroup(device, uniformArena, resolvedItem.ObjectBlock, resolvedItem.ObjectBindGroupLayoutRef, item.Transform);
			if (context.RecordingCommandBuffer) {
				context.RecordingCommandBuffer->RetainResource(resolvedItem.PipelineStateRef);
				context.RecordingCommandBuffer->RetainResource(frameBindGroup);
				context.RecordingCommandBuffer->RetainResource(resolvedItem.MaterialBindGroupRef);
				context.RecordingCommandBuffer->RetainResource(objectBindGroup);
			}

			if (resolvedItem.PipelineStateRef
				&& resolvedItem.VertexBinding.Buffer
				&& resolvedItem.IndexBinding.Buffer
				&& resolvedItem.IndexBinding.IndexCount > 0
				&& resolvedItem.MaterialBindGroupRef
				&& frameBindGroup
				&& objectBindGroup) {
				context.Commands->SetPipelineState(*resolvedItem.PipelineStateRef);
				context.Commands->SetBindGroup(0, *frameBindGroup);
				context.Commands->SetVertexBuffer(0, resolvedItem.VertexBinding);
				context.Commands->SetIndexBuffer(resolvedItem.IndexBinding);
				context.Commands->SetBindGroup(1, *resolvedItem.MaterialBindGroupRef);
				context.Commands->SetBindGroup(2, *objectBindGroup);
				context.Commands->DrawIndexed(resolvedItem.IndexBinding.IndexCount);
			} else {
				context.Diagnostics->push_back({
					RenderDiagnosticCode::MissingRhiDrawResources,
					item.SourceEntity,
					"Render item skipped because resolved RHI draw resources were incomplete"
				});
				++context.Stats->SkippedItems;
				continue;
			}

			++context.Stats->SubmittedItems;
			++context.Stats->DrawCalls;
		}
	}

}
