#include "enginepch.h"
#include "ForwardRenderPipeline.h"

#include "HuaEngine/Rendering/RenderPipeline/RenderResourceResolver.h"
#include "HuaEngine/Rendering/RHI/CommandList.h"
#include "HuaEngine/Rendering/RHI/FrameObjectBinding.h"
#include "HuaEngine/Rendering/RHI/RenderHardwareInterface.h"

namespace HE::Rendering {
	namespace {
		RenderGraphDiagnosticCode ToRenderGraphDiagnosticCode(PassGraphDiagnosticCode code) {
			switch (code) {
				case PassGraphDiagnosticCode::EmptyGraph:
					return RenderGraphDiagnosticCode::EmptyGraph;
				case PassGraphDiagnosticCode::EmptyPassName:
					return RenderGraphDiagnosticCode::EmptyPassName;
				case PassGraphDiagnosticCode::DuplicatePassName:
					return RenderGraphDiagnosticCode::DuplicatePassName;
				case PassGraphDiagnosticCode::MissingExecuteCallback:
					return RenderGraphDiagnosticCode::MissingExecuteCallback;
				case PassGraphDiagnosticCode::EmptyResourceName:
					return RenderGraphDiagnosticCode::EmptyResourceName;
				case PassGraphDiagnosticCode::DuplicateResourceAccess:
					return RenderGraphDiagnosticCode::DuplicateResourceAccess;
				case PassGraphDiagnosticCode::MissingResourceProducer:
					return RenderGraphDiagnosticCode::MissingResourceProducer;
				case PassGraphDiagnosticCode::DuplicateResourceWriter:
					return RenderGraphDiagnosticCode::DuplicateResourceWriter;
			}

			return RenderGraphDiagnosticCode::EmptyGraph;
		}
	}

	void BindTargetPass::Execute(RenderPassContext& context) {
		if (!context.View || !context.View->Target || !context.Commands || !context.Stats) {
			return;
		}

		++context.Stats->PassCount;
		context.Commands->BeginRenderTarget(*context.View->Target);
	}

	void ClearTargetPass::Execute(RenderPassContext& context) {
		if (!context.View || !context.Commands || !context.Stats) {
			return;
		}

		++context.Stats->PassCount;
		if (context.View->ClearColorBuffer) {
			context.Commands->ClearColor(context.View->ClearColor);
		}
	}

	void BeginRendererPass::Execute(RenderPassContext& context) {
		if (!context.View || !context.View->CameraRef || !context.Commands || !context.Stats) {
			return;
		}

		++context.Stats->PassCount;
		context.Commands->BeginFrame(*context.View->CameraRef);
	}

	void ForwardOpaquePass::Execute(RenderPassContext& context) {
		if (!context.RenderItems || !context.ResourceResolver || !context.Commands || !context.Stats || !context.Diagnostics) {
			return;
		}

		++context.Stats->PassCount;

		for (const auto& item : *context.RenderItems) {
			ResolvedRenderItem resolvedItem;
			if (!context.ResourceResolver->Resolve(item, resolvedItem, *context.Stats, *context.Diagnostics)) {
				++context.Stats->SkippedItems;
				continue;
			}

			if (resolvedItem.PipelineStateRef && resolvedItem.VertexBufferViewRef && resolvedItem.MaterialBindingRef) {
				context.Commands->SetPipelineState(*resolvedItem.PipelineStateRef);
				context.Commands->SetFrameBinding({ .ViewProjection = context.View->CameraRef->GetViewProjection() });
				context.Commands->SetVertexBufferView(*resolvedItem.VertexBufferViewRef);
				context.Commands->SetMaterialBinding(*resolvedItem.MaterialBindingRef);
				context.Commands->SetObjectBinding({ .Transform = item.Transform });
				context.Commands->DrawIndexed(resolvedItem.VertexBufferViewRef->GetDesc().IndexCount);
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

	void EndRendererPass::Execute(RenderPassContext& context) {
		if (!context.Commands || !context.Stats) {
			return;
		}

		++context.Stats->PassCount;
		context.Commands->EndFrame();
	}

	void UnbindTargetPass::Execute(RenderPassContext& context) {
		if (!context.View || !context.View->Target || !context.Commands || !context.Stats) {
			return;
		}

		++context.Stats->PassCount;
		context.Commands->EndRenderTarget();
	}

	void ForwardRenderPipeline::BuildGraph() {
		m_Graph.Reset();
		m_Graph.AddExternalInput("RenderTarget");
		m_Graph.AddExternalInput("CameraView");
		m_Graph.AddExternalInput("SceneItems");
		m_Graph.AddPass({
			.Name = "BindTarget",
			.Inputs = { "RenderTarget" },
			.Outputs = { "BoundRenderTarget" },
			.Execute = [this](RenderPassContext& context) {
				m_BindTargetPass.Execute(context);
			}
		});
		m_Graph.AddPass({
			.Name = "ClearTarget",
			.Inputs = { "BoundRenderTarget" },
			.Outputs = { "ClearedSceneColor" },
			.Execute = [this](RenderPassContext& context) {
				m_ClearTargetPass.Execute(context);
			}
		});
		m_Graph.AddPass({
			.Name = "BeginRenderer",
			.Inputs = { "CameraView" },
			.Outputs = { "RendererFrame" },
			.Execute = [this](RenderPassContext& context) {
				m_BeginRendererPass.Execute(context);
			}
		});
		m_Graph.AddPass({
			.Name = "ForwardOpaque",
			.Inputs = { "CameraView", "SceneItems", "RendererFrame", "ClearedSceneColor" },
			.Outputs = { "SceneColor" },
			.Execute = [this](RenderPassContext& context) {
				m_OpaquePass.Execute(context);
			}
		});
		m_Graph.AddPass({
			.Name = "EndRenderer",
			.Inputs = { "RendererFrame" },
			.Execute = [this](RenderPassContext& context) {
				m_EndRendererPass.Execute(context);
			}
		});
		m_Graph.AddPass({
			.Name = "UnbindTarget",
			.Inputs = { "BoundRenderTarget" },
			.Execute = [this](RenderPassContext& context) {
				m_UnbindTargetPass.Execute(context);
			}
		});
	}

	bool ForwardRenderPipeline::EnsureGraphCompiled(RenderResult& result) {
		if (m_Graph.GetPasses().empty()) {
			BuildGraph();
		}

		if (!m_Graph.IsCompiled() && !m_Graph.Compile()) {
			CopyGraphStateToResult(result);
			return false;
		}

		CopyGraphStateToResult(result);
		return true;
	}

	void ForwardRenderPipeline::CopyGraphStateToResult(RenderResult& result) const {
		const auto& graphStats = m_Graph.GetStats();
		result.GraphStats.ResourceCount = graphStats.ResourceCount;
		result.GraphStats.EdgeCount = graphStats.EdgeCount;
		result.GraphStats.ExternalInputCount = graphStats.ExternalInputCount;
		result.GraphStats.OutputCount = graphStats.OutputCount;

		result.GraphDiagnostics.clear();
		result.GraphDiagnostics.reserve(m_Graph.GetDiagnostics().size());
		for (const auto& diagnostic : m_Graph.GetDiagnostics()) {
			result.GraphDiagnostics.push_back({
				.Code = ToRenderGraphDiagnosticCode(diagnostic.Code),
				.PassName = diagnostic.PassName,
				.Message = diagnostic.Message
			});
		}
	}

	RenderResult ForwardRenderPipeline::Render(
		const RenderView& view,
		const std::vector<RenderItem>& renderItems,
		const RenderResourceResolver& resourceResolver) {
		RenderResult result;
		result.Stats.RenderItems = static_cast<uint32_t>(renderItems.size());
		result.Stats.VisibleItems = result.Stats.RenderItems;

		if (!view.CameraRef || !view.Target) {
			return result;
		}

		if (!EnsureGraphCompiled(result)) {
			return result;
		}

		auto& commandList = RenderHardwareInterface::GetDevice().GetImmediateCommandList();

		RenderPassContext passContext;
		passContext.View = &view;
		passContext.RenderItems = &renderItems;
		passContext.ResourceResolver = &resourceResolver;
		passContext.Commands = &commandList;
		passContext.Stats = &result.Stats;
		passContext.Diagnostics = &result.Diagnostics;

		const bool graphExecuted = m_Graph.Execute(passContext);

		result.Succeeded = graphExecuted;
		CopyGraphStateToResult(result);
		return result;
	}
}
