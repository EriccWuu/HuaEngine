#include "enginepch.h"
#include "ForwardRenderPipeline.h"

#include "HuaEngine/Rendering/RenderGraph/RenderGraphBuilder.h"
#include "HuaEngine/Rendering/RenderPipeline/RenderBindGroupBuilder.h"
#include "HuaEngine/Rendering/RenderPipeline/RenderResourceResolver.h"
#include "HuaEngine/Rendering/RHI/CommandBufferRecorder.h"
#include "HuaEngine/Rendering/RHI/CommandList.h"
#include "HuaEngine/Rendering/RHI/RenderPass.h"
#include "HuaEngine/Rendering/RHI/RenderHardwareInterface.h"

namespace HE::Rendering {
	namespace {
		RenderGraphResultDiagnosticCode ToRenderGraphResultDiagnosticCode(RenderGraphDiagnosticCode code) {
			switch (code) {
				case RenderGraphDiagnosticCode::EmptyGraph:
					return RenderGraphResultDiagnosticCode::EmptyGraph;
				case RenderGraphDiagnosticCode::EmptyPassName:
					return RenderGraphResultDiagnosticCode::EmptyPassName;
				case RenderGraphDiagnosticCode::DuplicatePassName:
					return RenderGraphResultDiagnosticCode::DuplicatePassName;
				case RenderGraphDiagnosticCode::MissingExecuteCallback:
					return RenderGraphResultDiagnosticCode::MissingExecuteCallback;
				case RenderGraphDiagnosticCode::EmptyResourceName:
					return RenderGraphResultDiagnosticCode::EmptyResourceName;
				case RenderGraphDiagnosticCode::InvalidResourceDescription:
					return RenderGraphResultDiagnosticCode::InvalidResourceDescription;
				case RenderGraphDiagnosticCode::InvalidResourceHandle:
					return RenderGraphResultDiagnosticCode::InvalidResourceHandle;
			case RenderGraphDiagnosticCode::InvalidResourceUsage:
				return RenderGraphResultDiagnosticCode::InvalidResourceUsage;
			case RenderGraphDiagnosticCode::InvalidPassType:
				return RenderGraphResultDiagnosticCode::InvalidPassType;
				case RenderGraphDiagnosticCode::DuplicateResourceAccess:
					return RenderGraphResultDiagnosticCode::DuplicateResourceAccess;
				case RenderGraphDiagnosticCode::MissingResourceProducer:
					return RenderGraphResultDiagnosticCode::MissingResourceProducer;
			case RenderGraphDiagnosticCode::DuplicateResourceWriter:
				return RenderGraphResultDiagnosticCode::DuplicateResourceWriter;
			case RenderGraphDiagnosticCode::CyclicDependency:
				return RenderGraphResultDiagnosticCode::CyclicDependency;
			}

			return RenderGraphResultDiagnosticCode::EmptyGraph;
		}
	}

	void ForwardRenderPipeline::BuildGraph(const RenderView& view) {
		RenderGraphBuilder graph(m_Graph);
		const auto clearColor = view.ClearColor;
		const auto clearColorBuffer = view.ClearColorBuffer;
		const auto viewportColor = view.Target->GetColorAttachmentTexture();
		const auto viewportColorHandle = graph.ImportTexture("ViewportColorAttachment", viewportColor);
		const auto viewportDepth = view.Target->GetDepthStencilAttachmentTexture();
		const auto viewportDepthHandle = viewportDepth
			? graph.ImportTexture("ViewportDepthAttachment", viewportDepth)
			: RenderGraphResourceHandle{};
		graph.Export(viewportColorHandle);
		const auto sceneColorHandle = graph.CreateTexture("SceneColor", {
				.Width = viewportColor->GetWidth(),
				.Height = viewportColor->GetHeight(),
				.Format = viewportColor->GetDesc().Format,
				.AttachmentGroup = "ForwardScene"
		});
		const auto sceneDepthHandle = graph.CreateTexture("SceneDepthAttachment", {
				.Width = viewportColor->GetWidth(),
				.Height = viewportColor->GetHeight(),
				.Format = RenderTargetTextureFormat::DEPTH24_STENCIL8,
				.AttachmentGroup = "ForwardScene"
		});
		m_OpaquePass.Configure(sceneColorHandle, sceneDepthHandle, viewportDepthHandle.IsValid(), clearColor, clearColorBuffer);
		m_PostProcessPass.Configure(sceneColorHandle, viewportColorHandle, clearColor);
		graph.AddPass(m_OpaquePass);
		graph.AddPass(m_PostProcessPass);
	}

	bool ForwardRenderPipeline::EnsureGraphCompiled(const RenderView& view, RenderResult& result) {
		BuildGraph(view);

		if (!m_Graph.Compile()) {
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
		result.GraphStats.OutputCount = graphStats.OutputCount;

		result.GraphDiagnostics.clear();
		result.GraphDiagnostics.reserve(m_Graph.GetDiagnostics().size());
		for (const auto& diagnostic : m_Graph.GetDiagnostics()) {
			result.GraphDiagnostics.push_back({
				.Code = ToRenderGraphResultDiagnosticCode(diagnostic.Code),
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

		if (!EnsureGraphCompiled(view, result)) {
			return result;
		}

		auto& device = RenderHardwareInterface::GetDevice();
		m_DeferredReleaseQueue.RetireCompleted();
		auto commandBuffer = device.CreateCommandBuffer({
			.Usage = CommandBufferUsage::Graphics,
			.DebugName = "ForwardRenderPipeline graph"
		});
		if (!commandBuffer || !commandBuffer->Begin()) {
			CopyGraphStateToResult(result);
			return result;
		}

		CommandBufferRecorder commandList(*commandBuffer);

		RenderPassContext passContext;
		passContext.View = &view;
		passContext.RenderItems = &renderItems;
		passContext.ResourceResolver = &resourceResolver;
		passContext.Commands = &commandList;
		passContext.RecordingCommandBuffer = commandBuffer.get();
		passContext.Device = &device;
		passContext.CompletedGraphicsFenceValue = device.GetGraphicsQueue().GetTimelineFence().GetCompletedValue();
		m_ResourceStates.Reset();
		passContext.ResourceStates = &m_ResourceStates;
		passContext.Stats = &result.Stats;
		passContext.Diagnostics = &result.Diagnostics;

		m_BeginRendererPass.Execute(passContext);
		const bool graphExecuted = m_Graph.Execute(passContext);
		m_EndRendererPass.Execute(passContext);
		const bool commandBufferEnded = commandBuffer->End();
		if (!graphExecuted || !commandList.Succeeded() || !commandBufferEnded) {
			CopyGraphStateToResult(result);
			return result;
		}

		const auto submitResult = device.GetGraphicsQueue().Submit(*commandBuffer);
		if (!submitResult) {
			CopyGraphStateToResult(result);
			return result;
		}
		m_Graph.ReleaseTransientResources(submitResult.SignalValue);
		m_DeferredReleaseQueue.Track(commandBuffer, submitResult.SignalFence, submitResult.SignalValue);

		result.Stats.GraphicsQueueSignalValue = submitResult.SignalValue;
		result.Stats.GraphicsQueueCompletedValue = submitResult.SignalFence
			? submitResult.SignalFence->GetCompletedValue()
			: device.GetGraphicsQueue().GetTimelineFence().GetCompletedValue();
		result.Stats.FramesInFlight = m_DeferredReleaseQueue.GetPendingCount();
		result.Succeeded = true;
		CopyGraphStateToResult(result);
		return result;
	}
}
