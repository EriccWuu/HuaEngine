#include "enginepch.h"
#include "ForwardRenderPipeline.h"

#include "HuaEngine/Rendering/RenderPipeline/RenderGraphBuilder.h"
#include "HuaEngine/Rendering/RenderPipeline/RenderBindGroupBuilder.h"
#include "HuaEngine/Rendering/RenderPipeline/RenderResourceResolver.h"
#include "HuaEngine/Rendering/RHI/CommandBufferRecorder.h"
#include "HuaEngine/Rendering/RHI/CommandList.h"
#include "HuaEngine/Rendering/RHI/RenderPass.h"
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
				case PassGraphDiagnosticCode::InvalidResourceDescription:
					return RenderGraphDiagnosticCode::InvalidResourceDescription;
				case PassGraphDiagnosticCode::InvalidResourceHandle:
					return RenderGraphDiagnosticCode::InvalidResourceHandle;
			case PassGraphDiagnosticCode::InvalidResourceUsage:
				return RenderGraphDiagnosticCode::InvalidResourceUsage;
			case PassGraphDiagnosticCode::InvalidPassType:
				return RenderGraphDiagnosticCode::InvalidPassType;
				case PassGraphDiagnosticCode::DuplicateResourceAccess:
					return RenderGraphDiagnosticCode::DuplicateResourceAccess;
				case PassGraphDiagnosticCode::MissingResourceProducer:
					return RenderGraphDiagnosticCode::MissingResourceProducer;
			case PassGraphDiagnosticCode::DuplicateResourceWriter:
				return RenderGraphDiagnosticCode::DuplicateResourceWriter;
			case PassGraphDiagnosticCode::CyclicDependency:
				return RenderGraphDiagnosticCode::CyclicDependency;
			}

			return RenderGraphDiagnosticCode::EmptyGraph;
		}
	}

	void BeginRendererPass::Execute(RenderPassContext& context) {
		if (!context.View || !context.View->CameraRef || !context.Commands || !context.Stats) {
			return;
		}

		++context.Stats->PassCount;
		context.Commands->BeginFrame();
	}

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

		auto frameBindGroup = CreateFrameBindGroup(RenderHardwareInterface::GetDevice(), context.View->CameraRef->GetViewProjection());
		if (!frameBindGroup) {
			context.Diagnostics->push_back({
				RenderDiagnosticCode::MissingRhiDrawResources,
				Entity{},
				"Forward opaque pass skipped because the frame bind group could not be created"
			});
			return;
		}
		if (context.RecordingCommandBuffer) {
			context.RecordingCommandBuffer->RetainResource(frameBindGroup);
		}

		for (const auto& item : *context.RenderItems) {
			ResolvedRenderItem resolvedItem;
			if (!context.ResourceResolver->Resolve(item, resolvedItem, *context.Stats, *context.Diagnostics)) {
				++context.Stats->SkippedItems;
				continue;
			}

			auto objectBindGroup = CreateObjectBindGroup(RenderHardwareInterface::GetDevice(), item.Transform);
			if (context.RecordingCommandBuffer) {
				context.RecordingCommandBuffer->RetainResource(resolvedItem.PipelineStateRef);
				context.RecordingCommandBuffer->RetainResource(resolvedItem.MaterialBindGroupRef);
				context.RecordingCommandBuffer->RetainResource(objectBindGroup);
			}

			if (resolvedItem.PipelineStateRef
				&& resolvedItem.VertexBinding.Buffer
				&& resolvedItem.IndexBinding.Buffer
				&& resolvedItem.IndexBinding.IndexCount > 0
				&& resolvedItem.MaterialBindGroupRef
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

	void PostProcessPass::Configure(
		RenderGraphResourceHandle sceneColor,
		RenderGraphResourceHandle output,
		const glm::vec4& clearColor) {
		m_SceneColor = sceneColor;
		m_Output = output;
		m_ClearColor = clearColor;
	}

	void PostProcessPass::Setup(RenderGraphPassBuilder& builder) {
		builder.Read(m_SceneColor, ResourceState::ShaderRead);
		builder.WriteColor(m_Output, LoadOp::Clear, StoreOp::Store, m_ClearColor);
	}

	void PostProcessPass::Execute(RenderPassContext& context) {
		if (!context.Device || !context.Commands || !context.GraphResources || !context.Stats) {
			return;
		}

		const auto* sceneColorResource = context.GraphResources->GetRuntimeResource(m_SceneColor);
		if (!sceneColorResource || !sceneColorResource->Texture) {
			return;
		}

		const float vertices[] = {
			-1.0f, -1.0f, 0.0f,
			 3.0f, -1.0f, 0.0f,
			-1.0f,  3.0f, 0.0f
		};
		const uint32_t indices[] = { 0, 1, 2 };
		const BufferLayout vertexLayout = {
			{ ShaderDataType::Float3, "a_Position" }
		};

		auto vertexBuffer = context.Device->CreateBuffer({
			.Usage = GpuBufferUsage::Vertex,
			.Size = sizeof(vertices),
			.Stride = 3 * sizeof(float)
		}, vertices);
		auto indexBuffer = context.Device->CreateBuffer({
			.Usage = GpuBufferUsage::Index,
			.Size = sizeof(indices),
			.Stride = sizeof(uint32_t)
		}, indices);
		auto shader = context.Device->CreateShaderProgram({
			.VertexSource = R"(
				#version 330 core
				layout(location = 0) in vec3 a_Position;
				out vec2 v_Uv;
				void main() {
					gl_Position = vec4(a_Position, 1.0);
					v_Uv = a_Position.xy * 0.5 + 0.5;
				}
			)",
			.FragmentSource = R"(
				#version 330 core
				in vec2 v_Uv;
				layout(location = 0) out vec4 color;
				uniform sampler2D u_SourceTexture;
				void main() {
					color = texture(u_SourceTexture, v_Uv);
				}
			)"
		});
		auto bindGroupLayout = context.Device->CreateBindGroupLayout({
			.Scope = BindGroupScope::Material,
			.Entries = {
				{ .Name = "u_SourceTexture", .Type = BindingValueType::TextureView, .Binding = 0 },
				{ .Name = "u_SourceSampler", .Type = BindingValueType::Sampler, .Binding = 1 }
			}
		});
		auto textureView = context.Device->CreateTextureView({ .Texture = sceneColorResource->Texture });
		auto sampler = context.Device->CreateSampler({
			.AddressU = SamplerAddressMode::ClampToEdge,
			.AddressV = SamplerAddressMode::ClampToEdge
		});
		if (!vertexBuffer || !indexBuffer || !shader || !bindGroupLayout || !textureView || !sampler) {
			return;
		}

		auto pipeline = context.Device->CreatePipelineState({
			.Shader = shader,
			.VertexLayout = vertexLayout,
			.Topology = PrimitiveTopology::TriangleList,
			.ColorTargets = { { .Format = sceneColorResource->Texture->GetDesc().Format } },
			.DepthStencil = {
				.Format = RenderTargetTextureFormat::None,
				.DepthTestEnabled = false,
				.DepthWriteEnabled = false
			},
			.Raster = { .Cull = CullMode::None },
			.BindGroupLayouts = { { .Slot = 0, .Layout = bindGroupLayout } }
		});
		auto bindGroup = context.Device->CreateBindGroup({
			.Layout = bindGroupLayout,
			.Entries = {
				{
					.Name = "u_SourceTexture",
					.Type = BindingValueType::TextureView,
					.Value = textureView,
					.Binding = 0,
					.TextureSlot = 0
				},
				{
					.Name = "u_SourceSampler",
					.Type = BindingValueType::Sampler,
					.Value = sampler,
					.Binding = 1,
					.TextureSlot = 0
				}
			}
		});
		if (!pipeline || !bindGroup) {
			return;
		}

		if (context.RecordingCommandBuffer) {
			context.RecordingCommandBuffer->RetainResource(vertexBuffer);
			context.RecordingCommandBuffer->RetainResource(indexBuffer);
			context.RecordingCommandBuffer->RetainResource(shader);
			context.RecordingCommandBuffer->RetainResource(bindGroupLayout);
			context.RecordingCommandBuffer->RetainResource(textureView);
			context.RecordingCommandBuffer->RetainResource(sampler);
			context.RecordingCommandBuffer->RetainResource(pipeline);
			context.RecordingCommandBuffer->RetainResource(bindGroup);
		}

		context.Commands->SetPipelineState(*pipeline);
		context.Commands->SetVertexBuffer(0, { .Buffer = vertexBuffer, .Stride = 3 * sizeof(float) });
		context.Commands->SetIndexBuffer({ .Buffer = indexBuffer, .Format = IndexFormat::UInt32, .IndexCount = 3 });
		context.Commands->SetBindGroup(0, *bindGroup);
		context.Commands->DrawIndexed(3);
		++context.Stats->DrawCalls;
		++context.Stats->PassCount;
	}

	void EndRendererPass::Execute(RenderPassContext& context) {
		if (!context.Commands || !context.Stats) {
			return;
		}

		++context.Stats->PassCount;
		context.Commands->EndFrame();
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
		graph.AddGraphicsPass("ForwardOpaque", m_OpaquePass);
		graph.AddGraphicsPass("PostProcess", m_PostProcessPass);
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
