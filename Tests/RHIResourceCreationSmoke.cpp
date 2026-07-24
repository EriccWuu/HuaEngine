#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "HuaEngine.h"
#include "HuaEngine/Core/ResourcePaths.h"
#include "HuaEngine/Rendering/RenderPipeline/PassGraph.h"
#include "HuaEngine/Rendering/RHI/CommandList.h"
#include "HuaEngine/Rendering/RHI/ResourceBarrier.h"
#include "HuaEngine/Rendering/RHI/ResourceStateTracker.h"
#include "HuaEngine/Rendering/RHI/RenderHardwareInterface.h"

namespace {
	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[RHIResourceCreationSmoke] " << message << std::endl;
			std::exit(1);
		}
	}

	HE::ApplicationSpecification MakeApplicationSpecification() {
		HE::ApplicationSpecification specification;
		specification.Name = "RHIResourceCreationSmoke";
		specification.EnableGuiLayer = false;
		specification.EnableWindow = true;
		return specification;
	}

	class SmokeApplication final : public HE::Application {
	public:
		SmokeApplication()
			: HE::Application(MakeApplicationSpecification()) {}
	};

	class BarrierCaptureCommandList final : public HE::Rendering::CommandList {
	public:
		std::vector<HE::Rendering::ResourceBarrier> Barriers;

		void BeginRenderPass(const HE::Rendering::RenderPassDesc&) override {}
		void EndRenderPass() override {}
		void ResourceBarrier(const HE::Rendering::ResourceBarrier& barrier) override { Barriers.push_back(barrier); }
		void BeginRenderTarget(HE::Rendering::RenderTarget&) override {}
		void ClearColor(const glm::vec4&) override {}
		void BeginFrame() override {}
		void SetPipelineState(HE::Rendering::PipelineState&) override {}
		void SetVertexBuffer(uint32_t, const HE::Rendering::VertexBufferBinding&) override {}
		void SetIndexBuffer(const HE::Rendering::IndexBufferBinding&) override {}
		void SetVertexBufferView(HE::Rendering::VertexBufferView&) override {}
		void SetBindGroup(uint32_t, HE::Rendering::BindGroup&) override {}
		void DrawIndexed(uint32_t) override {}
		void EndFrame() override {}
		void EndRenderTarget() override {}
	};
}

int main() {
	HE::Log::Init({ .EnableConsoleOutput = false });

	SmokeApplication application;
	application.Start();

	auto& device = HE::Rendering::RenderHardwareInterface::GetDevice();
	Require(device.GetDesc().Backend == HE::Rendering::RenderBackendType::OpenGL, "Expected default render backend to be OpenGL");
	Require(device.GetCapabilities().Backend == HE::Rendering::RenderBackendType::OpenGL, "Expected OpenGL device capabilities");
	Require(device.GetCapabilities().SupportsPipelineState, "Expected pipeline state support capability");
	Require(device.GetCapabilities().SupportsBindGroups, "Expected bind group support capability");
	Require(device.GetCapabilities().SupportsCommandSubmission, "Expected command submission support capability");
	Require(!HE::Rendering::RenderHardwareInterface::CreateRenderDevice({ .Backend = HE::Rendering::RenderBackendType::Null }), "Expected unimplemented null backend creation to fail");

	auto schemaMaterial = HE::Rendering::Material::Create("SchemaMaterial", HE::Rendering::MaterialType::Custom);
	Require(static_cast<bool>(schemaMaterial), "Expected schema material creation to succeed");
	schemaMaterial->AddParameter({ "u_Roughness", HE::Rendering::MaterialParameterType::Float, 0.5f });
	schemaMaterial->AddParameter({ "u_BaseColor", HE::Rendering::MaterialParameterType::Vec4, glm::vec4(1.0f) });
	const auto schema = schemaMaterial->GetBindingSchema();
	Require(schema.Entries.size() == 2, "Expected material binding schema entries");
	Require(schema.Entries[0].Name == "u_BaseColor", "Expected material binding schema to be sorted by name");
	Require(schema.Entries[0].Binding == 0, "Expected first material schema binding");
	Require(schema.Entries[1].Name == "u_Roughness", "Expected second material binding schema entry");
	Require(!schema.Signature.empty(), "Expected material binding schema signature");
	auto schemaInstance = schemaMaterial->CreateInstance();
	Require(static_cast<bool>(schemaInstance), "Expected schema material instance creation to succeed");
	schemaInstance->SetParameter("u_BaseColor", glm::vec4(0.25f, 0.5f, 0.75f, 1.0f));
	Require(schemaMaterial->GetBindingSchema().Signature == schema.Signature, "Expected instance overrides to preserve base material schema signature");

	float vertices[] = {
		-0.5f, -0.5f, 0.0f,
		 0.5f, -0.5f, 0.0f,
		 0.0f,  0.5f, 0.0f
	};
	HE::Rendering::GpuBufferDesc vertexDesc;
	vertexDesc.Usage = HE::Rendering::GpuBufferUsage::Vertex;
	vertexDesc.Size = sizeof(vertices);
	vertexDesc.Stride = 3 * sizeof(float);
	auto vertexBuffer = device.CreateBuffer(vertexDesc, vertices);
	Require(static_cast<bool>(vertexBuffer), "Expected vertex GPU buffer creation to succeed");
	Require(vertexBuffer->GetDesc().Usage == HE::Rendering::GpuBufferUsage::Vertex, "Expected vertex GPU buffer usage");
	Require(vertexBuffer->GetDesc().Size == sizeof(vertices), "Expected vertex GPU buffer size");

	uint32_t indices[] = { 0, 1, 2 };
	HE::Rendering::GpuBufferDesc indexDesc;
	indexDesc.Usage = HE::Rendering::GpuBufferUsage::Index;
	indexDesc.Size = sizeof(indices);
	indexDesc.Stride = sizeof(uint32_t);
	auto indexBuffer = device.CreateBuffer(indexDesc, indices);
	Require(static_cast<bool>(indexBuffer), "Expected index GPU buffer creation to succeed");
	Require(indexBuffer->GetDesc().Usage == HE::Rendering::GpuBufferUsage::Index, "Expected index GPU buffer usage");

	HE::Rendering::BufferLayout layout = {
		{ HE::Rendering::ShaderDataType::Float3, "a_Position" }
	};
	HE::Rendering::VertexBufferViewDesc viewDesc;
	viewDesc.VertexBuffer = vertexBuffer;
	viewDesc.IndexBuffer = indexBuffer;
	viewDesc.Layout = layout;
	viewDesc.IndexCount = 3;
	auto vertexBufferView = device.CreateVertexBufferView(viewDesc);
	Require(static_cast<bool>(vertexBufferView), "Expected vertex buffer view creation to succeed");
	Require(vertexBufferView->GetDesc().IndexCount == 3, "Expected vertex buffer view index count");

	HE::Rendering::VertexBufferBinding vertexBinding{
		.Buffer = vertexBuffer,
		.Offset = 0,
		.Stride = 3 * sizeof(float)
	};
	Require(vertexBinding.Buffer == vertexBuffer, "Expected vertex binding buffer to round-trip");
	Require(vertexBinding.Stride == 3 * sizeof(float), "Expected vertex binding stride");

	HE::Rendering::IndexBufferBinding indexBinding{
		.Buffer = indexBuffer,
		.Offset = 0,
		.Format = HE::Rendering::IndexFormat::UInt32,
		.IndexCount = 3
	};
	Require(indexBinding.Buffer == indexBuffer, "Expected index binding buffer to round-trip");
	Require(indexBinding.IndexCount == 3, "Expected index binding count");

	struct MatrixIntegerVertex {
		float Transform[16];
		int Ids[2];
	};
	MatrixIntegerVertex matrixIntegerVertices[] = {
		{ { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, -0.5f, -0.5f, 0.0f, 1.0f }, { 0, 10 } },
		{ { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,  0.5f, -0.5f, 0.0f, 1.0f }, { 1, 11 } },
		{ { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,  0.0f,  0.5f, 0.0f, 1.0f }, { 2, 12 } }
	};
	HE::Rendering::GpuBufferDesc matrixIntegerVertexDesc;
	matrixIntegerVertexDesc.Usage = HE::Rendering::GpuBufferUsage::Vertex;
	matrixIntegerVertexDesc.Size = sizeof(matrixIntegerVertices);
	matrixIntegerVertexDesc.Stride = sizeof(MatrixIntegerVertex);
	auto matrixIntegerVertexBuffer = device.CreateBuffer(matrixIntegerVertexDesc, matrixIntegerVertices);
	Require(static_cast<bool>(matrixIntegerVertexBuffer), "Expected matrix/integer vertex GPU buffer creation to succeed");

	HE::Rendering::BufferLayout matrixIntegerLayout = {
		{ HE::Rendering::ShaderDataType::Mat4, "a_Transform" },
		{ HE::Rendering::ShaderDataType::Int2, "a_Ids" }
	};
	HE::Rendering::VertexBufferViewDesc matrixIntegerViewDesc;
	matrixIntegerViewDesc.VertexBuffer = matrixIntegerVertexBuffer;
	matrixIntegerViewDesc.IndexBuffer = indexBuffer;
	matrixIntegerViewDesc.Layout = matrixIntegerLayout;
	matrixIntegerViewDesc.IndexCount = 3;
	auto matrixIntegerVertexBufferView = device.CreateVertexBufferView(matrixIntegerViewDesc);
	Require(static_cast<bool>(matrixIntegerVertexBufferView), "Expected matrix/integer vertex buffer view creation to succeed");

	HE::Rendering::RenderTargetSpecification frameBufferSpec;
	frameBufferSpec.Width = 64;
	frameBufferSpec.Height = 64;
	frameBufferSpec.Attachments = { HE::Rendering::RenderTargetTextureFormat::RGBA8, HE::Rendering::RenderTargetTextureFormat::DEPTH24_STENCIL8 };
	auto renderTarget = device.CreateRenderTarget({ .Specification = frameBufferSpec });
	Require(static_cast<bool>(renderTarget), "Expected render target creation to succeed");
	Require(renderTarget->GetSpecification().Width == 64, "Expected render target width");
	Require(renderTarget->GetSpecification().Height == 64, "Expected render target height");
	const auto colorAttachmentView = renderTarget->GetColorAttachmentView(0);
	Require(colorAttachmentView.NativeHandle != 0, "Expected color attachment native handle");
	Require(colorAttachmentView.Format == HE::Rendering::RenderTargetTextureFormat::RGBA8, "Expected color attachment format metadata");
	Require(colorAttachmentView.Width == 64 && colorAttachmentView.Height == 64, "Expected color attachment size metadata");
	Require(colorAttachmentView.Samples == 1, "Expected color attachment sample metadata");
	Require(colorAttachmentView.AttachmentIndex == 0, "Expected color attachment index metadata");
	const auto depthAttachmentView = renderTarget->GetDepthStencilAttachmentView();
	Require(depthAttachmentView.NativeHandle != 0, "Expected depth/stencil attachment native handle");
	Require(depthAttachmentView.Format == HE::Rendering::RenderTargetTextureFormat::DEPTH24_STENCIL8, "Expected depth/stencil attachment format metadata");
	Require(depthAttachmentView.Width == 64 && depthAttachmentView.Height == 64, "Expected depth/stencil attachment size metadata");
	Require(depthAttachmentView.Samples == 1, "Expected depth/stencil attachment sample metadata");
	const auto colorAttachmentTexture = renderTarget->GetColorAttachmentTexture(0);
	Require(static_cast<bool>(colorAttachmentTexture), "Expected color attachment texture resource");
	Require(colorAttachmentTexture->GetDesc().Format == HE::Rendering::RenderTargetTextureFormat::RGBA8, "Expected color attachment texture format");
	Require((colorAttachmentTexture->GetDesc().Usage & HE::Rendering::TextureUsageColorAttachment) != 0, "Expected color attachment usage");
	Require((colorAttachmentTexture->GetDesc().Usage & HE::Rendering::TextureUsageSampled) != 0, "Expected color attachment sampled usage");
	const auto depthAttachmentTexture = renderTarget->GetDepthStencilAttachmentTexture();
	Require(static_cast<bool>(depthAttachmentTexture), "Expected depth/stencil attachment texture resource");
	Require(depthAttachmentTexture->GetDesc().Format == HE::Rendering::RenderTargetTextureFormat::DEPTH24_STENCIL8, "Expected depth/stencil attachment texture format");
	Require((depthAttachmentTexture->GetDesc().Usage & HE::Rendering::TextureUsageDepthStencilAttachment) != 0, "Expected depth/stencil attachment usage");
	const auto colorAttachmentTextureView = renderTarget->GetColorAttachmentTextureView(0);
	Require(static_cast<bool>(colorAttachmentTextureView), "Expected color attachment texture view");
	Require(colorAttachmentTextureView->GetDesc().Texture == colorAttachmentTexture, "Expected color attachment texture view source");
	const auto depthAttachmentTextureView = renderTarget->GetDepthStencilAttachmentTextureView();
	Require(static_cast<bool>(depthAttachmentTextureView), "Expected depth/stencil attachment texture view");
	Require(depthAttachmentTextureView->GetDesc().Texture == depthAttachmentTexture, "Expected depth/stencil attachment texture view source");

	const auto texturePath = HE::ResourcePaths::ResolveEngineResourcePath("ret.png");
	auto texture = device.CreateTexture({ .SourcePath = texturePath.generic_string() });
	Require(static_cast<bool>(texture), "Expected texture resource creation to succeed");
	Require(texture->GetWidth() > 0 && texture->GetHeight() > 0, "Expected texture dimensions");
	Require(texture->GetDesc().Width == texture->GetWidth(), "Expected file-backed texture desc width to round-trip");
	Require(texture->GetDesc().Height == texture->GetHeight(), "Expected file-backed texture desc height to round-trip");
	Require(texture->GetDesc().Format == HE::Rendering::RenderTargetTextureFormat::RGBA8, "Expected file-backed texture format");
	Require((texture->GetDesc().Usage & HE::Rendering::TextureUsageSampled) != 0, "Expected file-backed texture sampled usage");

	auto emptyTexture = device.CreateTexture({
		.Width = 32,
		.Height = 16,
		.Format = HE::Rendering::RenderTargetTextureFormat::RGBA8,
		.Usage = HE::Rendering::TextureUsageSampled | HE::Rendering::TextureUsageCopyDst,
		.MipLevels = 1,
		.Samples = 1
	});
	Require(static_cast<bool>(emptyTexture), "Expected non-file-backed texture creation to succeed");
	Require(emptyTexture->GetWidth() == 32, "Expected non-file-backed texture width");
	Require(emptyTexture->GetHeight() == 16, "Expected non-file-backed texture height");
	Require(emptyTexture->GetDesc().Format == HE::Rendering::RenderTargetTextureFormat::RGBA8, "Expected non-file-backed texture format");
	Require((emptyTexture->GetDesc().Usage & HE::Rendering::TextureUsageCopyDst) != 0, "Expected non-file-backed texture usage");
	Require(emptyTexture->GetDesc().MipLevels == 1, "Expected non-file-backed texture mip levels");
	Require(emptyTexture->GetDesc().Samples == 1, "Expected non-file-backed texture sample count");
	Require(!device.CreateTexture({}), "Expected empty texture description to fail");
	Require(!device.CreateTexture({
		.Width = 32,
		.Height = 16,
		.Format = HE::Rendering::RenderTargetTextureFormat::None,
		.Usage = HE::Rendering::TextureUsageSampled
	}), "Expected texture without a concrete format to fail");
	Require(!device.CreateTexture({
		.Width = 32,
		.Height = 16,
		.Format = HE::Rendering::RenderTargetTextureFormat::RGBA8,
		.Usage = HE::Rendering::TextureUsageNone
	}), "Expected texture without usage flags to fail");
	auto textureView = device.CreateTextureView({
		.Texture = emptyTexture,
		.Format = HE::Rendering::RenderTargetTextureFormat::RGBA8,
		.BaseMipLevel = 0,
		.MipLevelCount = 1
	});
	Require(static_cast<bool>(textureView), "Expected texture view creation to succeed");
	Require(textureView->GetDesc().Texture == emptyTexture, "Expected texture view resource to round-trip");
	Require(textureView->GetDesc().Format == HE::Rendering::RenderTargetTextureFormat::RGBA8, "Expected texture view format to round-trip");
	auto sampler = device.CreateSampler({
		.MinFilter = HE::Rendering::SamplerFilter::Nearest,
		.MagFilter = HE::Rendering::SamplerFilter::Linear,
		.AddressU = HE::Rendering::SamplerAddressMode::ClampToEdge,
		.AddressV = HE::Rendering::SamplerAddressMode::Repeat,
		.AddressW = HE::Rendering::SamplerAddressMode::Repeat
	});
	Require(static_cast<bool>(sampler), "Expected sampler creation to succeed");
	Require(sampler->GetDesc().MinFilter == HE::Rendering::SamplerFilter::Nearest, "Expected sampler min filter to round-trip");
	Require(sampler->GetDesc().AddressU == HE::Rendering::SamplerAddressMode::ClampToEdge, "Expected sampler address mode to round-trip");
	auto sampledTextureLayout = device.CreateBindGroupLayout({
		.Scope = HE::Rendering::BindGroupScope::Material,
		.Entries = {
			{
				.Name = "u_Texture",
				.Type = HE::Rendering::BindingValueType::TextureView,
				.Binding = 0
			},
			{
				.Name = "u_TextureSampler",
				.Type = HE::Rendering::BindingValueType::Sampler,
				.Binding = 1
			}
		}
	});
	Require(static_cast<bool>(sampledTextureLayout), "Expected texture view/sampler layout creation to succeed");
	auto sampledTextureBindGroup = device.CreateBindGroup({
		.Layout = sampledTextureLayout,
		.Entries = {
			{
				.Name = "u_Texture",
				.Type = HE::Rendering::BindingValueType::TextureView,
				.Value = colorAttachmentTextureView,
				.Binding = 0,
				.TextureSlot = 0
			},
			{
				.Name = "u_TextureSampler",
				.Type = HE::Rendering::BindingValueType::Sampler,
				.Value = sampler,
				.Binding = 1,
				.TextureSlot = 0
			}
		}
	});
	Require(static_cast<bool>(sampledTextureBindGroup), "Expected texture view/sampler bind group creation to succeed");
	Require(std::get<HE::Ref<HE::Rendering::TextureView>>(sampledTextureBindGroup->GetDesc().Entries[0].Value) == colorAttachmentTextureView, "Expected bind group to retain render target attachment view");
	renderTarget->Resize(32, 48);
	Require(renderTarget->GetColorAttachmentTexture(0) == colorAttachmentTexture, "Expected resize to preserve color attachment texture identity");
	Require(colorAttachmentTexture->GetWidth() == 32 && colorAttachmentTexture->GetHeight() == 48, "Expected color attachment texture dimensions after resize");
	Require(colorAttachmentTextureView->GetDesc().Texture == colorAttachmentTexture, "Expected color attachment view source after resize");
	const auto resizedColorAttachmentView = renderTarget->GetColorAttachmentView(0);
	Require(resizedColorAttachmentView.Width == 32 && resizedColorAttachmentView.Height == 48, "Expected color attachment metadata after resize");
	Require(!device.CreateTextureView({}), "Expected empty texture view creation to fail");

	HE::Rendering::PassGraph attachmentSamplingGraph;
	const auto attachmentColorHandle = attachmentSamplingGraph.AddImportedResource({
		.Name = "AttachmentColor",
		.Kind = HE::Rendering::RenderGraphResourceKind::Texture,
		.Texture = {
			.Width = colorAttachmentTexture->GetWidth(),
			.Height = colorAttachmentTexture->GetHeight(),
			.Format = colorAttachmentTexture->GetDesc().Format
		},
		.RuntimeTexture = colorAttachmentTexture
	});
	bool writerPassUsedAttachmentView = false;
	bool readerPassUsedAttachmentTexture = false;
	HE::Ref<HE::Rendering::TextureView> sampledAttachmentView;
	attachmentSamplingGraph.AddPass({
		.Name = "WriteAttachment",
		.RenderPassAttachments = {
			{
				.Resource = attachmentColorHandle,
				.Kind = HE::Rendering::PassGraphRenderPassAttachmentKind::Color,
				.Load = HE::Rendering::LoadOp::Clear,
				.Store = HE::Rendering::StoreOp::Store,
				.ClearColor = { 0.2f, 0.3f, 0.4f, 1.0f }
			}
		},
		.Execute = [&](HE::Rendering::RenderPassContext& context) {
			const auto* runtimeResource = context.GraphResources->GetRuntimeResource(attachmentColorHandle);
			writerPassUsedAttachmentView = runtimeResource
				&& runtimeResource->Texture == colorAttachmentTexture
				&& context.GraphRenderPass
				&& context.GraphRenderPass->ColorAttachments.size() == 1
				&& context.GraphRenderPass->ColorAttachments[0].View
				&& context.GraphRenderPass->ColorAttachments[0].View->GetDesc().Texture == colorAttachmentTexture;
			context.Commands->BeginRenderPass(*context.GraphRenderPass);
			context.Commands->EndRenderPass();
		}
	});
	attachmentSamplingGraph.AddPass({
		.Name = "SampleAttachment",
		.Inputs = { "AttachmentColor" },
		.Execute = [&](HE::Rendering::RenderPassContext& context) {
			const auto handle = context.GraphResources->FindByName("AttachmentColor");
			const auto* runtimeResource = context.GraphResources->GetRuntimeResource(handle);
			if (runtimeResource && runtimeResource->Texture == colorAttachmentTexture) {
				sampledAttachmentView = context.Device->CreateTextureView({ .Texture = runtimeResource->Texture });
				readerPassUsedAttachmentTexture = static_cast<bool>(sampledAttachmentView);
			}
		}
	});
	Require(attachmentSamplingGraph.Compile(), "Expected attachment sampling graph compile to succeed");
	std::vector<HE::Rendering::PassGraphResourceBarrier> attachmentBarrierSequence;
	attachmentSamplingGraph.SetBarrierExecutor([&](const HE::Rendering::PassGraphResourceBarrier& barrier, HE::Rendering::RenderPassContext&) {
		attachmentBarrierSequence.push_back(barrier);
	});
	HE::Rendering::ResourceStateTracker attachmentResourceStates;
	HE::Rendering::RenderPassContext attachmentSamplingContext;
	attachmentSamplingContext.Device = &device;
	attachmentSamplingContext.Commands = &device.GetImmediateCommandList();
	attachmentSamplingContext.ResourceStates = &attachmentResourceStates;
	Require(attachmentSamplingGraph.Execute(attachmentSamplingContext), "Expected attachment sampling graph execute to succeed");
	Require(attachmentSamplingContext.GraphResources == nullptr, "Expected graph resource context to be restored after execute");
	Require(attachmentSamplingContext.GraphRenderPass == nullptr, "Expected graph render-pass context to be restored after execute");
	Require(writerPassUsedAttachmentView, "Expected writer pass to resolve the imported attachment texture");
	Require(readerPassUsedAttachmentTexture, "Expected reader pass to create a sampled view from the imported attachment texture");
	Require(sampledAttachmentView->GetDesc().Texture == colorAttachmentTexture, "Expected sampled attachment view source texture");
	Require(attachmentBarrierSequence.size() == 2, "Expected attachment graph to emit write and sampled-read barriers");
	Require(attachmentBarrierSequence[0].Before == HE::Rendering::ResourceState::Undefined && attachmentBarrierSequence[0].After == HE::Rendering::ResourceState::RenderTarget, "Expected attachment write barrier state transition");
	Require(attachmentBarrierSequence[1].Before == HE::Rendering::ResourceState::RenderTarget && attachmentBarrierSequence[1].After == HE::Rendering::ResourceState::ShaderRead, "Expected attachment sampled-read barrier state transition");
	Require(attachmentResourceStates.GetState(colorAttachmentTexture) == HE::Rendering::ResourceState::ShaderRead, "Expected attachment texture final shader-read state");
	device.GetImmediateCommandList().ResourceBarrier({
		.Texture = texture,
		.Before = HE::Rendering::ResourceState::Undefined,
		.After = HE::Rendering::ResourceState::ShaderRead
	});

	HE::Rendering::PassGraph runtimeResourceGraph;
	const auto importedGraphTexture = runtimeResourceGraph.AddImportedResource({
		.Name = "ImportedTexture",
		.Kind = HE::Rendering::RenderGraphResourceKind::Texture,
		.Texture = {
			.Width = emptyTexture->GetDesc().Width,
			.Height = emptyTexture->GetDesc().Height,
			.Format = emptyTexture->GetDesc().Format
		},
		.RuntimeTexture = emptyTexture
	});
	const auto transientGraphTexture = runtimeResourceGraph.AddTransientResource({
		.Name = "TransientTexture",
		.Kind = HE::Rendering::RenderGraphResourceKind::Texture,
		.Texture = {
			.Width = 16,
			.Height = 8,
			.Format = HE::Rendering::RenderTargetTextureFormat::RGBA8
		}
	});
	runtimeResourceGraph.AddPass({
		.Name = "RuntimeResourcePass",
		.Inputs = { "ImportedTexture" },
		.Outputs = { "TransientTexture" },
		.Execute = [](HE::Rendering::RenderPassContext&) {}
	});
	Require(runtimeResourceGraph.Compile(), "Expected runtime resource graph compile to succeed");
	HE::Rendering::RenderPassContext runtimeResourceContext;
	runtimeResourceContext.Device = &device;
	Require(runtimeResourceGraph.Execute(runtimeResourceContext), "Expected runtime resource graph execute to succeed");
	const auto* importedRuntimeResource = runtimeResourceGraph.GetResourceAllocator().GetRuntimeResource(importedGraphTexture);
	Require(importedRuntimeResource && importedRuntimeResource->Texture == emptyTexture, "Expected imported graph texture to preserve runtime texture binding");
	const auto* transientRuntimeResource = runtimeResourceGraph.GetResourceAllocator().GetRuntimeResource(transientGraphTexture);
	Require(transientRuntimeResource && transientRuntimeResource->Texture, "Expected transient graph texture to allocate a runtime texture");
	Require(transientRuntimeResource->Texture->GetDesc().Width == 16, "Expected transient graph texture width");
	Require(transientRuntimeResource->Texture->GetDesc().Height == 8, "Expected transient graph texture height");
	Require(transientRuntimeResource->Texture->GetDesc().Format == HE::Rendering::RenderTargetTextureFormat::RGBA8, "Expected transient graph texture format");

	HE::Rendering::PassGraph stateTrackedGraph;
	stateTrackedGraph.AddImportedResource({
		.Name = "TrackedImportedTexture",
		.Kind = HE::Rendering::RenderGraphResourceKind::Texture,
		.Texture = {
			.Width = emptyTexture->GetDesc().Width,
			.Height = emptyTexture->GetDesc().Height,
			.Format = emptyTexture->GetDesc().Format
		},
		.RuntimeTexture = emptyTexture
	});
	stateTrackedGraph.AddPass({
		.Name = "ReadTrackedImportedTexture",
		.Inputs = { "TrackedImportedTexture" },
		.Execute = [](HE::Rendering::RenderPassContext&) {}
	});
	Require(stateTrackedGraph.Compile(), "Expected state tracked graph compile to succeed");
	BarrierCaptureCommandList barrierCaptureCommands;
	HE::Rendering::ResourceStateTracker resourceStates;
	HE::Rendering::RenderPassContext stateTrackedContext;
	stateTrackedContext.Device = &device;
	stateTrackedContext.Commands = &barrierCaptureCommands;
	stateTrackedContext.ResourceStates = &resourceStates;
	Require(stateTrackedGraph.Execute(stateTrackedContext), "Expected state tracked graph execute to succeed");
	Require(barrierCaptureCommands.Barriers.size() == 1, "Expected first graph execute to emit one resource barrier");
	Require(barrierCaptureCommands.Barriers[0].Texture == emptyTexture, "Expected emitted barrier to reference imported runtime texture");
	Require(barrierCaptureCommands.Barriers[0].Before == HE::Rendering::ResourceState::Undefined, "Expected first barrier before state");
	Require(barrierCaptureCommands.Barriers[0].After == HE::Rendering::ResourceState::ShaderRead, "Expected first barrier after state");
	Require(stateTrackedGraph.Execute(stateTrackedContext), "Expected repeated state tracked graph execute to succeed");
	Require(barrierCaptureCommands.Barriers.size() == 1, "Expected repeated graph execute to avoid duplicate same-state barrier");

	const std::string vertexSource = R"(
		#version 330 core
		layout(location = 0) in vec3 a_Position;
		void main() {
			gl_Position = vec4(a_Position, 1.0);
		}
	)";
	const std::string fragmentSource = R"(
		#version 330 core
		layout(location = 0) out vec4 color;
		void main() {
			color = vec4(1.0);
		}
	)";
	auto shaderProgram = device.CreateShaderProgram({
		.VertexSource = vertexSource,
		.FragmentSource = fragmentSource
	});
	Require(static_cast<bool>(shaderProgram), "Expected shader program creation to succeed");

	auto pipelineState = device.CreatePipelineState({
		.Shader = shaderProgram,
		.VertexLayout = layout,
		.Topology = HE::Rendering::PrimitiveTopology::TriangleList
	});
	Require(static_cast<bool>(pipelineState), "Expected pipeline state creation to succeed");
	Require(pipelineState->GetDesc().Shader == shaderProgram, "Expected pipeline state shader");
	Require(pipelineState->GetDesc().Topology == HE::Rendering::PrimitiveTopology::TriangleList, "Expected triangle list pipeline topology");
	Require(pipelineState->GetDesc().ColorTargets.size() == 1, "Expected default pipeline color target contract");
	Require(pipelineState->GetDesc().ColorTargets[0].Format == HE::Rendering::RenderTargetTextureFormat::RGBA8, "Expected default pipeline color target format");
	Require(!pipelineState->GetDesc().ColorTargets[0].BlendEnabled, "Expected default pipeline blend disabled");
	Require(pipelineState->GetDesc().DepthStencil.Format == HE::Rendering::RenderTargetTextureFormat::DEPTH24_STENCIL8, "Expected default pipeline depth/stencil format");
	Require(pipelineState->GetDesc().Raster.Cull == HE::Rendering::CullMode::Back, "Expected default pipeline raster cull mode");
	Require(!device.CreatePipelineState({}), "Expected empty pipeline state creation to fail");

	auto bindGroupLayout = device.CreateBindGroupLayout({
		.Scope = HE::Rendering::BindGroupScope::Material,
		.Entries = {
			{
				.Name = "u_Color",
				.Type = HE::Rendering::BindingValueType::Float4,
				.Binding = 0
			}
		}
	});
	Require(static_cast<bool>(bindGroupLayout), "Expected bind group layout creation to succeed");
	auto bindGroup = device.CreateBindGroup({
		.Layout = bindGroupLayout,
		.Entries = {
			{
				.Name = "u_Color",
				.Type = HE::Rendering::BindingValueType::Float4,
				.Value = glm::vec4(1.0f),
				.Binding = 0
			}
		}
	});
	Require(static_cast<bool>(bindGroup), "Expected bind group creation to succeed");
	Require(bindGroup->GetDesc().Layout == bindGroupLayout, "Expected bind group layout to round-trip");

	auto commandBuffer = device.CreateCommandBuffer({
		.Usage = HE::Rendering::CommandBufferUsage::Graphics,
		.DebugName = "RHIResourceCreationSmoke empty command buffer"
	});
	Require(static_cast<bool>(commandBuffer), "Expected command buffer creation to succeed");
	Require(commandBuffer->GetDesc().Usage == HE::Rendering::CommandBufferUsage::Graphics, "Expected graphics command buffer usage");
	Require(commandBuffer->GetDesc().DebugName == "RHIResourceCreationSmoke empty command buffer", "Expected command buffer debug name");
	device.GetGraphicsQueue().Submit(*commandBuffer);

	Require(!device.CreateCommandBuffer({ .Usage = HE::Rendering::CommandBufferUsage::Invalid }), "Expected invalid command buffer creation to fail");

	auto contractedPipelineState = device.CreatePipelineState({
		.Shader = shaderProgram,
		.VertexLayout = layout,
		.Topology = HE::Rendering::PrimitiveTopology::TriangleList,
		.BindGroupLayouts = {
			{
				.Slot = 1,
				.Layout = bindGroupLayout
			}
		}
	});
	Require(static_cast<bool>(contractedPipelineState), "Expected contracted pipeline state creation to succeed");
	Require(contractedPipelineState->GetDesc().BindGroupLayouts.size() == 1, "Expected pipeline bind group layout contract");
	Require(contractedPipelineState->GetDesc().BindGroupLayouts[0].Slot == 1, "Expected material bind group slot contract");
	Require(contractedPipelineState->GetDesc().BindGroupLayouts[0].Layout == bindGroupLayout, "Expected material bind group layout contract");

	auto renderStatePipeline = device.CreatePipelineState({
		.Shader = shaderProgram,
		.VertexLayout = layout,
		.Topology = HE::Rendering::PrimitiveTopology::TriangleList,
		.ColorTargets = {
			{
				.Format = HE::Rendering::RenderTargetTextureFormat::RGBA8,
				.BlendEnabled = true,
				.WriteMask = HE::Rendering::ColorWriteMaskAll
			}
		},
		.DepthStencil = {
			.Format = HE::Rendering::RenderTargetTextureFormat::DEPTH24_STENCIL8,
			.DepthTestEnabled = true,
			.DepthWriteEnabled = true,
			.DepthCompare = HE::Rendering::CompareOp::LessEqual
		},
		.Raster = {
			.Cull = HE::Rendering::CullMode::None,
			.FrontFaceMode = HE::Rendering::FrontFace::CounterClockwise,
			.Fill = HE::Rendering::FillMode::Solid
		}
	});
	Require(static_cast<bool>(renderStatePipeline), "Expected explicit render state pipeline creation to succeed");
	Require(renderStatePipeline->GetDesc().ColorTargets[0].BlendEnabled, "Expected pipeline blend state to round-trip");
	Require(renderStatePipeline->GetDesc().DepthStencil.DepthCompare == HE::Rendering::CompareOp::LessEqual, "Expected depth compare state to round-trip");
	Require(renderStatePipeline->GetDesc().Raster.Cull == HE::Rendering::CullMode::None, "Expected raster state to round-trip");

	Require(!device.CreatePipelineState({
		.Shader = shaderProgram,
		.VertexLayout = layout,
		.Topology = HE::Rendering::PrimitiveTopology::TriangleList,
		.ColorTargets = {}
	}), "Expected pipeline without color targets to fail");
	Require(!device.CreatePipelineState({
		.Shader = shaderProgram,
		.VertexLayout = layout,
		.Topology = HE::Rendering::PrimitiveTopology::TriangleList,
		.ColorTargets = {
			{
				.Format = HE::Rendering::RenderTargetTextureFormat::None
			}
		}
	}), "Expected pipeline with empty color target format to fail");
	Require(!device.CreatePipelineState({
		.Shader = shaderProgram,
		.VertexLayout = layout,
		.Topology = HE::Rendering::PrimitiveTopology::TriangleList,
		.ColorTargets = {
			{
				.Format = HE::Rendering::RenderTargetTextureFormat::DEPTH24_STENCIL8
			}
		}
	}), "Expected pipeline with depth format as color target to fail");
	Require(!device.CreatePipelineState({
		.Shader = shaderProgram,
		.VertexLayout = layout,
		.Topology = HE::Rendering::PrimitiveTopology::TriangleList,
		.DepthStencil = {
			.Format = HE::Rendering::RenderTargetTextureFormat::RGBA8
		}
	}), "Expected pipeline with color format as depth/stencil target to fail");

	Require(!device.CreatePipelineState({
		.Shader = shaderProgram,
		.VertexLayout = layout,
		.Topology = HE::Rendering::PrimitiveTopology::TriangleList,
		.BindGroupLayouts = {
			{
				.Slot = 1,
				.Layout = bindGroupLayout
			},
			{
				.Slot = 1,
				.Layout = bindGroupLayout
			}
		}
	}), "Expected duplicate pipeline bind group layout slots to fail");
	Require(!device.CreatePipelineState({
		.Shader = shaderProgram,
		.VertexLayout = layout,
		.Topology = HE::Rendering::PrimitiveTopology::TriangleList,
		.BindGroupLayouts = {
			{
				.Slot = 1,
				.Layout = nullptr
			}
		}
	}), "Expected null pipeline bind group layout to fail");

	Require(!device.CreateBindGroupLayout({}), "Expected empty bind group layout creation to fail");
	Require(!device.CreateBindGroup({}), "Expected empty bind group creation to fail");

	HE::Rendering::GpuBufferDesc invalidBufferDesc;
	invalidBufferDesc.Usage = HE::Rendering::GpuBufferUsage::Vertex;
	invalidBufferDesc.Size = 0;
	Require(!device.CreateBuffer(invalidBufferDesc, nullptr), "Expected zero-sized GPU buffer creation to fail");

	std::cout << "RHIResourceCreationSmoke passed" << std::endl;
	return 0;
}
