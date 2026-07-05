#include "enginepch.h"
#include "ForwardRenderPipeline.h"

#include "HuaEngine/Rendering/RenderCommand.h"
#include "HuaEngine/Rendering/RenderPipeline/RenderResourceResolver.h"
#include "HuaEngine/Rendering/Renderer.h"

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

	void ForwardOpaquePass::Execute(RenderPassContext& context) {
		if (!context.RenderItems || !context.ResourceResolver || !context.Stats || !context.Diagnostics) {
			return;
		}

		++context.Stats->PassCount;

		for (const auto& item : *context.RenderItems) {
			ResolvedRenderItem resolvedItem;
			if (!context.ResourceResolver->Resolve(item, resolvedItem, *context.Stats, *context.Diagnostics)) {
				++context.Stats->SkippedItems;
				continue;
			}

			Renderer::Submit(resolvedItem.MaterialInstanceRef, resolvedItem.VertexArrayRef, item.Transform);
			++context.Stats->SubmittedItems;
			++context.Stats->DrawCalls;
		}
	}

	void ForwardRenderPipeline::BuildGraph() {
		m_Graph.Reset();
		m_Graph.AddExternalInput("CameraView");
		m_Graph.AddExternalInput("SceneItems");
		m_Graph.AddPass({
			.Name = "ForwardOpaque",
			.Inputs = { "CameraView", "SceneItems" },
			.Outputs = { "SceneColor" },
			.Execute = [this](RenderPassContext& context) {
				m_OpaquePass.Execute(context);
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

		if (!EnsureGraphCompiled(result)) {
			return result;
		}

		if (!view.CameraRef || !view.Target) {
			return result;
		}

		view.Target->Bind();
		if (view.ClearColorBuffer) {
			RenderCommand::SetClearColor(view.ClearColor);
			RenderCommand::Clear();
		}

		RenderPassContext passContext;
		passContext.View = &view;
		passContext.RenderItems = &renderItems;
		passContext.ResourceResolver = &resourceResolver;
		passContext.Stats = &result.Stats;
		passContext.Diagnostics = &result.Diagnostics;

		Renderer::Begin(view.CameraRef);
		const bool graphExecuted = m_Graph.Execute(passContext);
		Renderer::End();
		view.Target->Unbind();

		result.Succeeded = graphExecuted;
		CopyGraphStateToResult(result);
		return result;
	}
}
