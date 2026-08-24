#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <glad/glad.h>

#include "HuaEngine.h"
#include "HuaEngine/Rendering/RenderCamera.h"
#include "HuaEngine/Rendering/RenderGraph/RenderGraphBuilder.h"
#include "HuaEngine/Rendering/RHI/CommandList.h"
#include "HuaEngine/Rendering/RHI/RenderPass.h"
#include "HuaEngine/Rendering/RHI/RenderHardwareInterface.h"
#include "HuaEngine/Rendering/RHI/ResourceStateTracker.h"

namespace {
	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[RHICommandListBindingSmoke] " << message << std::endl;
			std::exit(1);
		}
	}

	HE::ApplicationSpecification MakeApplicationSpecification() {
		HE::ApplicationSpecification specification;
		specification.Name = "RHICommandListBindingSmoke";
		specification.EnableGuiLayer = false;
		specification.EnableWindow = true;
		return specification;
	}

	class SmokeApplication final : public HE::Application {
	public:
		SmokeApplication()
			: HE::Application(MakeApplicationSpecification()) {}
	};

	class ManualFence final : public HE::Rendering::Fence {
	public:
		uint64_t GetCompletedValue() const override { return m_CompletedValue; }
		void SetCompletedValue(uint64_t value) { m_CompletedValue = value; }

	private:
		uint64_t m_CompletedValue = 0;
	};

	constexpr HE::Rendering::RenderTargetPixelRGBA8 kExpectedClearPixel{ 26, 26, 26, 255 };
	constexpr HE::Rendering::RenderTargetPixelRGBA8 kExpectedFragmentPixel{ 230, 51, 26, 255 };
	constexpr uint8_t kPixelTolerance = 3;

	bool PixelNear(
		const HE::Rendering::RenderTargetPixelRGBA8& pixel,
		const HE::Rendering::RenderTargetPixelRGBA8& expected,
		uint8_t tolerance) {
		const auto nearChannel = [tolerance](uint8_t actual, uint8_t target) {
			const int delta = static_cast<int>(actual) - static_cast<int>(target);
			return delta >= -static_cast<int>(tolerance) && delta <= static_cast<int>(tolerance);
		};

		return nearChannel(pixel.R, expected.R)
			&& nearChannel(pixel.G, expected.G)
			&& nearChannel(pixel.B, expected.B)
			&& nearChannel(pixel.A, expected.A);
	}

	void VerifyRenderTargetSamples(const HE::Ref<HE::Rendering::RenderTarget>& renderTarget) {
		const auto& actualTargetSpec = renderTarget->GetSpecification();
		const auto fragmentPixel = renderTarget->ReadPixelRGBA8(0, actualTargetSpec.Width / 2, actualTargetSpec.Height / 2);
		const auto clearPixel = renderTarget->ReadPixelRGBA8(0, actualTargetSpec.Width / 8, actualTargetSpec.Height / 8);

		Require(
			PixelNear(fragmentPixel, kExpectedFragmentPixel, kPixelTolerance),
			"Expected triangle-covered sample to match the fragment shader color");
		Require(
			PixelNear(clearPixel, kExpectedClearPixel, kPixelTolerance),
			"Expected sample outside the triangle to keep the clear color");
	}

	void VerifyRenderTargetCleared(const HE::Ref<HE::Rendering::RenderTarget>& renderTarget) {
		const auto& actualTargetSpec = renderTarget->GetSpecification();
		const auto centerPixel = renderTarget->ReadPixelRGBA8(0, actualTargetSpec.Width / 2, actualTargetSpec.Height / 2);

		Require(
			PixelNear(centerPixel, kExpectedClearPixel, kPixelTolerance),
			"Expected pipeline switch without rebinding frame/object bind groups to skip drawing");
	}

	void VerifyRenderTargetCenterCleared(const HE::Ref<HE::Rendering::RenderTarget>& renderTarget, const std::string& message) {
		const auto& actualTargetSpec = renderTarget->GetSpecification();
		const auto centerPixel = renderTarget->ReadPixelRGBA8(0, actualTargetSpec.Width / 2, actualTargetSpec.Height / 2);

		Require(PixelNear(centerPixel, kExpectedClearPixel, kPixelTolerance), message);
	}

	std::string ReadSourceFile(const std::filesystem::path& path) {
		std::ifstream stream(path);
		if (!stream) {
			return {};
		}

		std::stringstream buffer;
		buffer << stream.rdbuf();
		return buffer.str();
	}

	bool CommandListAvoidsRendererSpecificState() {
		const auto root = std::filesystem::current_path();
		const auto commandList = ReadSourceFile(root / "HuaEngine" / "src" / "HuaEngine" / "Rendering" / "RHI" / "CommandList.h");
		const auto openGLCommandListHeader = ReadSourceFile(root / "HuaEngine" / "src" / "Platform" / "OpenGL" / "RHI" / "OpenGLRenderDevice.h");
		const auto openGLCommandListSource = ReadSourceFile(root / "HuaEngine" / "src" / "Platform" / "OpenGL" / "RHI" / "OpenGLRenderDevice.cpp");

		return !commandList.empty()
			&& !openGLCommandListHeader.empty()
			&& !openGLCommandListSource.empty()
			&& commandList.find("Camera") == std::string::npos
			&& commandList.find("BeginFrame(Camera") == std::string::npos
			&& commandList.find("BeginFrame()") != std::string::npos
			&& openGLCommandListHeader.find("m_HasFrameBindGroup") == std::string::npos
			&& openGLCommandListHeader.find("m_HasObjectBindGroup") == std::string::npos
			&& openGLCommandListSource.find("m_HasFrameBindGroup") == std::string::npos
			&& openGLCommandListSource.find("m_HasObjectBindGroup") == std::string::npos
			&& openGLCommandListSource.find("BindGroupScope::Frame") == std::string::npos
			&& openGLCommandListSource.find("BindGroupScope::Object") == std::string::npos;
	}
}

int main() {
	HE::Log::Init({ .EnableConsoleOutput = false });

	SmokeApplication application;
	application.Start();
	Require(CommandListAvoidsRendererSpecificState(), "Expected CommandList to avoid camera and frame/object-specific bind group state");

	auto& device = HE::Rendering::RenderHardwareInterface::GetDevice();

	float vertices[] = {
		-0.5f, -0.5f, 0.0f,
		 0.5f, -0.5f, 0.0f,
		 0.0f,  0.5f, 0.0f
	};

	uint32_t indices[] = { 0, 1, 2 };

	HE::Rendering::GpuBufferDesc vertexDesc;
	vertexDesc.Usage = HE::Rendering::GpuBufferUsage::Vertex;
	vertexDesc.Size = sizeof(vertices);
	vertexDesc.Stride = 3 * sizeof(float);
	auto vertexBuffer = device.CreateBuffer(vertexDesc, vertices);
	Require(static_cast<bool>(vertexBuffer), "Expected vertex buffer creation to succeed");

	HE::Rendering::GpuBufferDesc indexDesc;
	indexDesc.Usage = HE::Rendering::GpuBufferUsage::Index;
	indexDesc.Size = sizeof(indices);
	indexDesc.Stride = sizeof(uint32_t);
	auto indexBuffer = device.CreateBuffer(indexDesc, indices);
	Require(static_cast<bool>(indexBuffer), "Expected index buffer creation to succeed");

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

	HE::Rendering::RenderTargetSpecification targetSpec;
	targetSpec.Width = 64;
	targetSpec.Height = 64;
	targetSpec.Attachments = {
		HE::Rendering::RenderTargetTextureFormat::RGBA8,
		HE::Rendering::RenderTargetTextureFormat::DEPTH24_STENCIL8
	};
	auto renderTarget = device.CreateRenderTarget({ .Specification = targetSpec });
	Require(static_cast<bool>(renderTarget), "Expected render target creation to succeed");

	const std::string vertexSource = R"(
		#version 330 core
		layout(location = 0) in vec3 a_Position;
		layout(std140) uniform FrameData { mat4 u_ViewProjection; };
		layout(std140) uniform ObjectData { mat4 u_Transform; };
		void main() {
			gl_Position = u_ViewProjection * u_Transform * vec4(a_Position, 1.0);
		}
	)";

	const std::string fragmentSource = R"(
		#version 330 core
		layout(location = 0) out vec4 color;
		layout(std140) uniform MaterialData { vec4 u_Color; };
		void main() {
			color = u_Color;
		}
	)";

	HE::Rendering::ShaderGpuInterface gpuInterface;
	gpuInterface.Resources = {
		{ "FrameData", HE::Rendering::ShaderResourceType::ConstantBuffer, 0, 0, 1, HE::Rendering::ShaderStageVertex },
		{ "MaterialData", HE::Rendering::ShaderResourceType::ConstantBuffer, 1, 0, 1, HE::Rendering::ShaderStageFragment },
		{ "ObjectData", HE::Rendering::ShaderResourceType::ConstantBuffer, 2, 0, 1, HE::Rendering::ShaderStageVertex }
	};
	gpuInterface.ConstantBuffers = {
		{ "FrameData", 0, 0, 64, {{ "u_ViewProjection", HE::Rendering::ShaderValueType::Float4x4, 0, 64, 16, 0, true }} },
		{ "MaterialData", 1, 0, 16, {{ "u_Color", HE::Rendering::ShaderValueType::Float4, 0, 16 }} },
		{ "ObjectData", 2, 0, 64, {{ "u_Transform", HE::Rendering::ShaderValueType::Float4x4, 0, 64, 16, 0, true }} }
	};
	HE::Rendering::ShaderResourceMap resourceMap;
	resourceMap.UniformBlocks = {
		{ .Name = "FrameData", .Set = 0, .Binding = 0, .BindingPoint = 0, .Size = 64, .StageMask = HE::Rendering::ShaderStageVertex, .Members = {{ .Name = "u_ViewProjection", .Offset = 0, .Size = 64 }} },
		{ .Name = "MaterialData", .Set = 1, .Binding = 0, .BindingPoint = 1, .Size = 16, .StageMask = HE::Rendering::ShaderStageFragment, .Members = {{ .Name = "u_Color", .Offset = 0, .Size = 16 }} },
		{ .Name = "ObjectData", .Set = 2, .Binding = 0, .BindingPoint = 2, .Size = 64, .StageMask = HE::Rendering::ShaderStageVertex, .Members = {{ .Name = "u_Transform", .Offset = 0, .Size = 64 }} }
	};
	HE::Rendering::ShaderProgramDesc shaderDesc;
	Require(HE::Rendering::BuildOpenGlShaderProgramDesc(vertexSource, fragmentSource, std::move(gpuInterface), std::move(resourceMap), shaderDesc).Succeeded(), "Expected shader descriptor creation to succeed");
	auto shaderProgram = device.CreateShaderProgram(shaderDesc);
	Require(static_cast<bool>(shaderProgram), "Expected shader program creation to succeed");

	HE::Rendering::RenderCamera camera;
	const glm::mat4 viewProjection = camera.GetViewProjection();
	const glm::vec4 materialColor(0.9f, 0.2f, 0.1f, 1.0f);
	const glm::mat4 objectTransform(1.0f);
	auto frameBuffer = device.CreateBuffer({ .Usage = HE::Rendering::GpuBufferUsage::Uniform, .Size = 64, .Stride = 16 }, &viewProjection[0][0]);
	auto materialBuffer = device.CreateBuffer({ .Usage = HE::Rendering::GpuBufferUsage::Uniform, .Size = 16, .Stride = 16 }, &materialColor[0]);
	auto objectBuffer = device.CreateBuffer({ .Usage = HE::Rendering::GpuBufferUsage::Uniform, .Size = 64, .Stride = 16 }, &objectTransform[0][0]);
	Require(frameBuffer && materialBuffer && objectBuffer, "Expected uniform buffer creation to succeed");

	auto frameBindGroupLayout = device.CreateBindGroupLayout({
		.Scope = HE::Rendering::BindGroupScope::Frame,
		.Entries = {
			{
				.Name = "FrameData",
				.Type = HE::Rendering::BindingValueType::UniformBuffer,
				.Binding = 0,
				.Visibility = HE::Rendering::ShaderStageVertex,
				.MinBindingSize = 64
			}
		},
		.InterfaceDigest = shaderDesc.Interface.Digest
	});
	Require(static_cast<bool>(frameBindGroupLayout), "Expected frame bind group layout creation to succeed");
	auto frameBindGroup = device.CreateBindGroup({
		.Layout = frameBindGroupLayout,
		.Entries = {
			{
				.Name = "FrameData",
				.Type = HE::Rendering::BindingValueType::UniformBuffer,
				.Value = frameBuffer,
				.Binding = 0,
				.Size = 64
			}
		}
	});
	Require(static_cast<bool>(frameBindGroup), "Expected frame bind group creation to succeed");

	auto materialBindGroupLayout = device.CreateBindGroupLayout({
		.Scope = HE::Rendering::BindGroupScope::Material,
		.Entries = {
			{
				.Name = "MaterialData",
				.Type = HE::Rendering::BindingValueType::UniformBuffer,
				.Binding = 1,
				.Visibility = HE::Rendering::ShaderStageFragment,
				.MinBindingSize = 16
			}
		},
		.InterfaceDigest = shaderDesc.Interface.Digest
	});
	Require(static_cast<bool>(materialBindGroupLayout), "Expected material bind group layout creation to succeed");
	auto materialBindGroup = device.CreateBindGroup({
		.Layout = materialBindGroupLayout,
		.Entries = {
			{
				.Name = "MaterialData",
				.Type = HE::Rendering::BindingValueType::UniformBuffer,
				.Value = materialBuffer,
				.Binding = 1,
				.Size = 16
			}
		}
	});
	Require(static_cast<bool>(materialBindGroup), "Expected material bind group creation to succeed");

	auto objectBindGroupLayout = device.CreateBindGroupLayout({
		.Scope = HE::Rendering::BindGroupScope::Object,
		.Entries = {
			{
				.Name = "ObjectData",
				.Type = HE::Rendering::BindingValueType::UniformBuffer,
				.Binding = 2,
				.Visibility = HE::Rendering::ShaderStageVertex,
				.MinBindingSize = 64
			}
		},
		.InterfaceDigest = shaderDesc.Interface.Digest
	});
	Require(static_cast<bool>(objectBindGroupLayout), "Expected object bind group layout creation to succeed");
	auto objectBindGroup = device.CreateBindGroup({
		.Layout = objectBindGroupLayout,
		.Entries = {
			{
				.Name = "ObjectData",
				.Type = HE::Rendering::BindingValueType::UniformBuffer,
				.Value = objectBuffer,
				.Binding = 2,
				.Size = 64
			}
		}
	});
	Require(static_cast<bool>(objectBindGroup), "Expected object bind group creation to succeed");

	auto wrongObjectBindGroupLayout = device.CreateBindGroupLayout({
		.Scope = HE::Rendering::BindGroupScope::Material,
		.Entries = {
			{
				.Name = "ObjectData",
				.Type = HE::Rendering::BindingValueType::UniformBuffer,
				.Binding = 2,
				.Visibility = HE::Rendering::ShaderStageVertex,
				.MinBindingSize = 64
			}
		},
		.InterfaceDigest = shaderDesc.Interface.Digest
	});
	Require(static_cast<bool>(wrongObjectBindGroupLayout), "Expected wrong object bind group layout creation to succeed");
	auto wrongObjectBindGroup = device.CreateBindGroup({
		.Layout = wrongObjectBindGroupLayout,
		.Entries = {
			{
				.Name = "ObjectData",
				.Type = HE::Rendering::BindingValueType::UniformBuffer,
				.Value = objectBuffer,
				.Binding = 2,
				.Size = 64
			}
		}
	});
	Require(static_cast<bool>(wrongObjectBindGroup), "Expected wrong object bind group creation to succeed");

	auto pipelineState = device.CreatePipelineState({
		.Shader = shaderProgram,
		.VertexLayout = layout,
		.Topology = HE::Rendering::PrimitiveTopology::TriangleList,
		.DepthStencil = {
			.Format = HE::Rendering::RenderTargetTextureFormat::None,
			.DepthTestEnabled = false,
			.DepthWriteEnabled = false
		},
		.BindGroupLayouts = {
			{
				.Slot = 0,
				.Layout = frameBindGroupLayout
			},
			{
				.Slot = 1,
				.Layout = materialBindGroupLayout
			},
			{
				.Slot = 2,
				.Layout = objectBindGroupLayout
			}
		}
	});
	Require(static_cast<bool>(pipelineState), "Expected pipeline state creation to succeed");

	auto renderStateBackendPipeline = device.CreatePipelineState({
		.Shader = shaderProgram,
		.VertexLayout = layout,
		.Topology = HE::Rendering::PrimitiveTopology::TriangleList,
		.ColorTargets = {
			{
				.Format = HE::Rendering::RenderTargetTextureFormat::RGBA8,
				.BlendEnabled = true,
				.SrcColor = HE::Rendering::BlendFactor::SrcAlpha,
				.DstColor = HE::Rendering::BlendFactor::OneMinusSrcAlpha,
				.SrcAlpha = HE::Rendering::BlendFactor::One,
				.DstAlpha = HE::Rendering::BlendFactor::Zero,
				.WriteMask = HE::Rendering::ColorWriteMaskRed | HE::Rendering::ColorWriteMaskAlpha
			}
		},
		.DepthStencil = {
			.Format = HE::Rendering::RenderTargetTextureFormat::DEPTH24_STENCIL8,
			.DepthTestEnabled = false,
			.DepthWriteEnabled = false,
			.DepthCompare = HE::Rendering::CompareOp::Always
		},
		.Raster = {
			.Cull = HE::Rendering::CullMode::None,
			.FrontFaceMode = HE::Rendering::FrontFace::Clockwise,
			.Fill = HE::Rendering::FillMode::Wireframe
		},
		.BindGroupLayouts = {
			{ .Slot = 0, .Layout = frameBindGroupLayout },
			{ .Slot = 1, .Layout = materialBindGroupLayout },
			{ .Slot = 2, .Layout = objectBindGroupLayout }
		}
	});
	Require(static_cast<bool>(renderStateBackendPipeline), "Expected backend render state pipeline creation to succeed");

	auto mismatchedColorTargetPipeline = device.CreatePipelineState({
		.Shader = shaderProgram,
		.VertexLayout = layout,
		.Topology = HE::Rendering::PrimitiveTopology::TriangleList,
		.ColorTargets = {
			{
				.Format = HE::Rendering::RenderTargetTextureFormat::RED_INTEGER
			}
		},
		.BindGroupLayouts = {
			{
				.Slot = 0,
				.Layout = frameBindGroupLayout
			},
			{
				.Slot = 1,
				.Layout = materialBindGroupLayout
			},
			{
				.Slot = 2,
				.Layout = objectBindGroupLayout
			}
		}
	});
	Require(static_cast<bool>(mismatchedColorTargetPipeline), "Expected mismatched color target pipeline creation to succeed");

	HE::Rendering::RenderTargetSpecification samplingTargetSpec;
	samplingTargetSpec.Width = 64;
	samplingTargetSpec.Height = 64;
	samplingTargetSpec.Attachments = { HE::Rendering::RenderTargetTextureFormat::RGBA8 };
	auto samplingSourceTarget = device.CreateRenderTarget({ .Specification = samplingTargetSpec });
	auto samplingDestinationTarget = device.CreateRenderTarget({ .Specification = samplingTargetSpec });
	Require(static_cast<bool>(samplingSourceTarget) && static_cast<bool>(samplingDestinationTarget), "Expected sampling source and destination targets to succeed");

	const std::string samplingVertexSource = R"(
		#version 330 core
		layout(location = 0) in vec3 a_Position;
		void main() {
			gl_Position = vec4(a_Position, 1.0);
		}
	)";
	const std::string samplingFragmentSource = R"(
		#version 330 core
		layout(location = 0) out vec4 color;
		uniform sampler2D u_SourceTexture;
		void main() {
			color = texture(u_SourceTexture, vec2(0.5, 0.5));
		}
	)";
	HE::Rendering::ShaderGpuInterface samplingInterface;
	samplingInterface.Resources = {
		{ "u_SourceTexture", HE::Rendering::ShaderResourceType::Texture2D, 1, 0, 1, HE::Rendering::ShaderStageFragment },
		{ "u_SourceSampler", HE::Rendering::ShaderResourceType::Sampler, 1, 1, 1, HE::Rendering::ShaderStageFragment }
	};
	HE::Rendering::ShaderResourceMap samplingResourceMap;
	samplingResourceMap.Textures = {{
		.TextureName = "u_SourceTexture",
		.SamplerName = "u_SourceSampler",
		.UniformName = "u_SourceTexture",
		.TextureSet = 1,
		.TextureBinding = 0,
		.SamplerSet = 1,
		.SamplerBinding = 1,
		.TextureUnit = 0,
		.StageMask = HE::Rendering::ShaderStageFragment
	}};
	HE::Rendering::ShaderProgramDesc samplingShaderDesc;
	Require(HE::Rendering::BuildOpenGlShaderProgramDesc(samplingVertexSource, samplingFragmentSource, std::move(samplingInterface), std::move(samplingResourceMap), samplingShaderDesc).Succeeded(), "Expected attachment sampling shader descriptor");
	auto samplingShaderProgram = device.CreateShaderProgram(samplingShaderDesc);
	Require(static_cast<bool>(samplingShaderProgram), "Expected attachment sampling shader creation to succeed");
	auto samplingBindGroupLayout = device.CreateBindGroupLayout({
		.Scope = HE::Rendering::BindGroupScope::Material,
		.Entries = {{
			.Name = "u_SourceTexture",
			.Type = HE::Rendering::BindingValueType::Texture,
			.Binding = 0,
			.Visibility = HE::Rendering::ShaderStageFragment
		}},
		.InterfaceDigest = samplingShaderDesc.Interface.Digest
	});
	Require(static_cast<bool>(samplingBindGroupLayout), "Expected attachment sampling bind group layout creation to succeed");
	auto samplingPipeline = device.CreatePipelineState({
		.Shader = samplingShaderProgram,
		.VertexLayout = layout,
		.Topology = HE::Rendering::PrimitiveTopology::TriangleList,
		.DepthStencil = {
			.Format = HE::Rendering::RenderTargetTextureFormat::None,
			.DepthTestEnabled = false,
			.DepthWriteEnabled = false
		},
		.BindGroupLayouts = {
			{
				.Slot = 1,
				.Layout = samplingBindGroupLayout
			}
		}
	});
	Require(static_cast<bool>(samplingPipeline), "Expected attachment sampling pipeline creation to succeed");

	auto& commands = device.GetImmediateCommandList();
	const glm::vec4 sampledSourceClearColor{ 0.25f, 0.5f, 0.75f, 1.0f };
	const HE::Rendering::RenderTargetPixelRGBA8 expectedSampledPixel{ 64, 128, 191, 255 };
	HE::Rendering::RenderGraph attachmentSamplingGraph;
	HE::Rendering::RenderGraphBuilder attachmentSamplingBuilder(attachmentSamplingGraph);
	const auto sourceAttachmentHandle = attachmentSamplingBuilder.ImportTexture("SourceAttachment", samplingSourceTarget->GetColorAttachmentTexture());
	bool readerBoundRuntimeAttachment = false;
	attachmentSamplingBuilder.AddPass("WriteSourceAttachment", HE::Rendering::RenderGraphPassType::Graphics, [&](HE::Rendering::RenderGraphPassBuilder& pass) {
		pass.Write(sourceAttachmentHandle, HE::Rendering::ResourceState::RenderTarget);
		pass.SetExecute([&](HE::Rendering::RenderPassContext& context) {
			context.Commands->BeginRenderPass({
				.ColorAttachments = {
					{
						.View = samplingSourceTarget->GetColorAttachmentTextureView(),
						.Load = HE::Rendering::LoadOp::Clear,
						.Store = HE::Rendering::StoreOp::Store,
						.ClearColor = sampledSourceClearColor
					}
				}
			});
			context.Commands->EndRenderPass();
		});
	});
	attachmentSamplingBuilder.AddPass("SampleSourceAttachment", HE::Rendering::RenderGraphPassType::Graphics, [&](HE::Rendering::RenderGraphPassBuilder& pass) {
		pass.Read(sourceAttachmentHandle, HE::Rendering::ResourceState::ShaderRead);
		pass.SetExecute([&](HE::Rendering::RenderPassContext& context) {
			const auto* runtimeResource = context.GraphResources->GetRuntimeResource(sourceAttachmentHandle);
			if (!runtimeResource || !runtimeResource->Texture) {
				return;
			}

			auto sampledBindGroup = context.Device->CreateBindGroup({
				.Layout = samplingBindGroupLayout,
				.Entries = {{
					.Name = "u_SourceTexture",
					.Type = HE::Rendering::BindingValueType::Texture,
					.Value = runtimeResource->Texture,
					.Binding = 0,
					.TextureSlot = 0
				}}
			});
			if (!sampledBindGroup) {
				return;
			}

			readerBoundRuntimeAttachment = std::get<HE::Ref<HE::Rendering::TextureResource>>(sampledBindGroup->GetDesc().Entries[0].Value) == runtimeResource->Texture;
			context.Commands->BeginRenderPass({
				.ColorAttachments = {
					{
						.View = samplingDestinationTarget->GetColorAttachmentTextureView(),
						.Load = HE::Rendering::LoadOp::Clear,
						.Store = HE::Rendering::StoreOp::Store,
						.ClearColor = { 0.0f, 0.0f, 0.0f, 1.0f }
					}
				}
			});
			context.Commands->BeginFrame();
			context.Commands->SetPipelineState(*samplingPipeline);
			context.Commands->SetVertexBufferView(*vertexBufferView);
			context.Commands->SetBindGroup(1, *sampledBindGroup);
			context.Commands->DrawIndexed(vertexBufferView->GetDesc().IndexCount);
			context.Commands->EndFrame();
			context.Commands->EndRenderPass();
		});
	});
	Require(attachmentSamplingGraph.Compile(), "Expected attachment sampling graph compile to succeed");
	HE::Rendering::ResourceStateTracker attachmentSamplingResourceStates;
	HE::Rendering::RenderPassContext attachmentSamplingContext;
	attachmentSamplingContext.Device = &device;
	attachmentSamplingContext.Commands = &commands;
	attachmentSamplingContext.ResourceStates = &attachmentSamplingResourceStates;
	Require(attachmentSamplingGraph.Execute(attachmentSamplingContext), "Expected attachment sampling graph execute to succeed");
	Require(readerBoundRuntimeAttachment, "Expected reader pass to bind the graph runtime attachment texture");
	Require(attachmentSamplingResourceStates.GetState(samplingSourceTarget->GetColorAttachmentTexture()) == HE::Rendering::ResourceState::ShaderRead, "Expected sampled source attachment state");
	const auto sampledPixel = samplingDestinationTarget->ReadPixelRGBA8(0, samplingTargetSpec.Width / 2, samplingTargetSpec.Height / 2);
	Require(PixelNear(sampledPixel, expectedSampledPixel, kPixelTolerance), "Expected destination center pixel to match sampled source attachment color");

	commands.BeginFrame();
	commands.SetPipelineState(*renderStateBackendPipeline);
	Require(!glIsEnabled(GL_CULL_FACE), "Expected pipeline cull none to disable GL_CULL_FACE");
	Require(!glIsEnabled(GL_DEPTH_TEST), "Expected pipeline depth disabled to disable GL_DEPTH_TEST");
	Require(glIsEnabled(GL_BLEND), "Expected pipeline blend enabled to enable GL_BLEND");

	GLboolean depthWriteMask = GL_TRUE;
	glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWriteMask);
	Require(depthWriteMask == GL_FALSE, "Expected pipeline depth write disabled to clear GL_DEPTH_WRITEMASK");

	GLboolean colorWriteMask[4] = { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };
	glGetBooleanv(GL_COLOR_WRITEMASK, colorWriteMask);
	Require(
		colorWriteMask[0] == GL_TRUE
			&& colorWriteMask[1] == GL_FALSE
			&& colorWriteMask[2] == GL_FALSE
			&& colorWriteMask[3] == GL_TRUE,
		"Expected pipeline color write mask to map to GL_COLOR_WRITEMASK");

	GLint frontFace = 0;
	glGetIntegerv(GL_FRONT_FACE, &frontFace);
	Require(frontFace == GL_CW, "Expected pipeline front face to map to GL_FRONT_FACE");

	GLint polygonMode[2] = { 0, 0 };
	glGetIntegerv(GL_POLYGON_MODE, polygonMode);
	Require(polygonMode[0] == GL_LINE && polygonMode[1] == GL_LINE, "Expected pipeline fill mode to map to GL_POLYGON_MODE");
	commands.EndFrame();

	commands.BeginRenderPass({
		.ColorAttachments = {
			{
				.Target = renderTarget,
				.AttachmentIndex = 0,
				.Load = HE::Rendering::LoadOp::Clear,
				.Store = HE::Rendering::StoreOp::Store,
				.ClearColor = { 0.1f, 0.1f, 0.1f, 1.0f }
			}
		}
	});
	commands.BeginFrame();
	commands.SetPipelineState(*pipelineState);
	commands.SetBindGroup(0, *frameBindGroup);
	commands.SetVertexBufferView(*vertexBufferView);
	commands.SetBindGroup(1, *materialBindGroup);
	commands.SetBindGroup(2, *objectBindGroup);
	commands.DrawIndexed(vertexBufferView->GetDesc().IndexCount);
	commands.EndFrame();
	commands.EndRenderPass();
	VerifyRenderTargetSamples(renderTarget);

	commands.BeginRenderPass({
		.ColorAttachments = {
			{
				.View = renderTarget->GetColorAttachmentTextureView(0),
				.Load = HE::Rendering::LoadOp::Clear,
				.Store = HE::Rendering::StoreOp::Store,
				.ClearColor = { 0.1f, 0.1f, 0.1f, 1.0f }
			}
		}
	});
	commands.BeginFrame();
	commands.SetPipelineState(*mismatchedColorTargetPipeline);
	commands.SetBindGroup(0, *frameBindGroup);
	commands.SetVertexBufferView(*vertexBufferView);
	commands.SetBindGroup(1, *materialBindGroup);
	commands.SetBindGroup(2, *objectBindGroup);
	commands.DrawIndexed(vertexBufferView->GetDesc().IndexCount);
	commands.EndFrame();
	commands.EndRenderPass();
	VerifyRenderTargetCenterCleared(renderTarget, "Expected color target format mismatch to skip draw");

	commands.BeginRenderPass({
		.ColorAttachments = {
			{
				.Target = renderTarget,
				.AttachmentIndex = 0,
				.Load = HE::Rendering::LoadOp::Clear,
				.Store = HE::Rendering::StoreOp::Store,
				.ClearColor = { 0.1f, 0.1f, 0.1f, 1.0f }
			}
		}
	});
	commands.BeginFrame();
	commands.SetPipelineState(*pipelineState);
	commands.SetVertexBuffer(0, {
		.Buffer = vertexBuffer,
		.Offset = 0,
		.Stride = 3 * sizeof(float)
	});
	commands.SetIndexBuffer({
		.Buffer = indexBuffer,
		.Offset = 0,
		.Format = HE::Rendering::IndexFormat::UInt32,
		.IndexCount = 3
	});
	commands.SetBindGroup(0, *frameBindGroup);
	commands.SetBindGroup(1, *materialBindGroup);
	commands.SetBindGroup(2, *objectBindGroup);
	commands.DrawIndexed(3);
	commands.EndFrame();
	commands.EndRenderPass();
	VerifyRenderTargetSamples(renderTarget);

	commands.BeginRenderPass({
		.ColorAttachments = {
			{
				.Target = renderTarget,
				.AttachmentIndex = 0,
				.Load = HE::Rendering::LoadOp::Clear,
				.Store = HE::Rendering::StoreOp::Store,
				.ClearColor = { 0.1f, 0.1f, 0.1f, 1.0f }
			}
		}
	});
	commands.BeginFrame();
	commands.SetPipelineState(*pipelineState);
	commands.SetBindGroup(0, *frameBindGroup);
	commands.SetVertexBufferView(*vertexBufferView);
	commands.SetBindGroup(1, *materialBindGroup);
	commands.SetBindGroup(2, *objectBindGroup);
	commands.SetPipelineState(*pipelineState);
	commands.DrawIndexed(vertexBufferView->GetDesc().IndexCount);
	commands.EndFrame();
	commands.EndRenderPass();
	VerifyRenderTargetCleared(renderTarget);

	commands.BeginRenderTarget(*renderTarget);
	commands.ClearColor({ 0.1f, 0.1f, 0.1f, 1.0f });
	commands.BeginFrame();
	commands.SetPipelineState(*pipelineState);
	commands.SetBindGroup(0, *frameBindGroup);
	commands.SetVertexBufferView(*vertexBufferView);
	commands.SetBindGroup(2, *objectBindGroup);
	commands.SetBindGroup(1, *materialBindGroup);
	commands.DrawIndexed(vertexBufferView->GetDesc().IndexCount);
	commands.EndFrame();
	commands.EndRenderTarget();
	VerifyRenderTargetSamples(renderTarget);

	commands.BeginRenderPass({
		.ColorAttachments = {
			{
				.Target = renderTarget,
				.AttachmentIndex = 0,
				.Load = HE::Rendering::LoadOp::Clear,
				.Store = HE::Rendering::StoreOp::Store,
				.ClearColor = { 0.1f, 0.1f, 0.1f, 1.0f }
			}
		}
	});
	commands.BeginFrame();
	commands.SetPipelineState(*pipelineState);
	commands.SetBindGroup(0, *frameBindGroup);
	commands.SetVertexBufferView(*vertexBufferView);
	commands.SetBindGroup(1, *materialBindGroup);
	commands.SetBindGroup(2, *wrongObjectBindGroup);
	commands.DrawIndexed(vertexBufferView->GetDesc().IndexCount);
	commands.EndFrame();
	commands.EndRenderPass();
	VerifyRenderTargetCleared(renderTarget);

	auto unfinishedCommandBuffer = device.CreateCommandBuffer({
		.Usage = HE::Rendering::CommandBufferUsage::Graphics,
		.DebugName = "RHICommandListBindingSmoke unfinished recorded commands"
	});
	Require(static_cast<bool>(unfinishedCommandBuffer), "Expected unfinished command buffer creation to succeed");
	Require(unfinishedCommandBuffer->Begin(), "Expected unfinished command buffer begin to succeed");
	Require(unfinishedCommandBuffer->IsRecording(), "Expected unfinished command buffer to report recording state");
	Require(!unfinishedCommandBuffer->IsExecutable(), "Expected unfinished command buffer to not be executable");
	const auto unfinishedSubmit = device.GetGraphicsQueue().Submit(*unfinishedCommandBuffer);
	Require(!unfinishedSubmit, "Expected recording command buffer submit to fail");
	Require(unfinishedSubmit.SignalValue == 0, "Expected failed submit to have no signal value");
	unfinishedCommandBuffer->Reset();
	Require(!unfinishedCommandBuffer->IsRecording(), "Expected reset command buffer to clear recording state");
	Require(!unfinishedCommandBuffer->IsExecutable(), "Expected reset command buffer to clear executable state");

	auto recordedCommandBuffer = device.CreateCommandBuffer({
		.Usage = HE::Rendering::CommandBufferUsage::Graphics,
		.DebugName = "RHICommandListBindingSmoke recorded draw"
	});
	Require(static_cast<bool>(recordedCommandBuffer), "Expected recorded command buffer creation to succeed");
	Require(recordedCommandBuffer->Begin(), "Expected recorded command buffer begin to succeed");
	Require(recordedCommandBuffer->RecordBeginFrame(), "Expected command buffer to record frame begin");
	Require(recordedCommandBuffer->RecordResourceBarrier({
		.Texture = renderTarget->GetColorAttachmentTexture(),
		.Before = HE::Rendering::ResourceState::Undefined,
		.After = HE::Rendering::ResourceState::RenderTarget
	}), "Expected command buffer to record resource barrier");
	Require(recordedCommandBuffer->RecordBeginRenderPass({
		.ColorAttachments = {
			{
				.Target = renderTarget,
				.AttachmentIndex = 0,
				.Load = HE::Rendering::LoadOp::Clear,
				.Store = HE::Rendering::StoreOp::Store,
				.ClearColor = { 0.1f, 0.1f, 0.1f, 1.0f }
			}
		}
	}), "Expected command buffer to record render pass begin");
	Require(recordedCommandBuffer->RecordSetPipelineState(*pipelineState), "Expected command buffer to record pipeline state");
	Require(recordedCommandBuffer->RecordSetVertexBuffer(0, {
		.Buffer = vertexBuffer,
		.Offset = 0,
		.Stride = 3 * sizeof(float)
	}), "Expected command buffer to record vertex buffer binding");
	Require(recordedCommandBuffer->RecordSetIndexBuffer({
		.Buffer = indexBuffer,
		.Offset = 0,
		.Format = HE::Rendering::IndexFormat::UInt32,
		.IndexCount = 3
	}), "Expected command buffer to record index buffer binding");
	Require(recordedCommandBuffer->RecordSetBindGroup(0, *frameBindGroup), "Expected command buffer to record frame bind group");
	Require(recordedCommandBuffer->RecordSetBindGroup(1, *materialBindGroup), "Expected command buffer to record material bind group");
	Require(recordedCommandBuffer->RecordSetBindGroup(2, *objectBindGroup), "Expected command buffer to record object bind group");
	Require(recordedCommandBuffer->RecordDrawIndexed(3), "Expected command buffer to record indexed draw");
	Require(recordedCommandBuffer->RecordEndRenderPass(), "Expected command buffer to record render pass end");
	Require(recordedCommandBuffer->RecordEndFrame(), "Expected command buffer to record frame end");
	Require(recordedCommandBuffer->End(), "Expected recorded command buffer end to succeed");
	Require(!recordedCommandBuffer->IsRecording(), "Expected ended command buffer to clear recording state");
	Require(recordedCommandBuffer->IsExecutable(), "Expected ended command buffer to be executable");
	const auto firstSubmit = device.GetGraphicsQueue().Submit(*recordedCommandBuffer);
	Require(firstSubmit, "Expected executable command buffer submit to succeed");
	Require(firstSubmit.SignalValue > 0, "Expected successful submit to return a signal value");
	Require(firstSubmit.SignalFence != nullptr, "Expected successful submit to return a signal fence");
	Require(firstSubmit.SignalFence->GetCompletedValue() <= firstSubmit.SignalValue, "Expected fence completion not to exceed the submitted signal value");
	const auto secondSubmit = device.GetGraphicsQueue().Submit(*recordedCommandBuffer);
	Require(secondSubmit, "Expected repeated executable command buffer submit to succeed");
	Require(secondSubmit.SignalValue == firstSubmit.SignalValue + 1, "Expected queue signal value to increase monotonically");
	Require(secondSubmit.SignalFence == firstSubmit.SignalFence, "Expected graphics queue to reuse its timeline fence");
	for (uint32_t poll = 0; poll < 100000 && secondSubmit.SignalFence->GetCompletedValue() < secondSubmit.SignalValue; ++poll) {}
	Require(secondSubmit.SignalFence->GetCompletedValue() == secondSubmit.SignalValue, "Expected graphics GPU fence to complete after polling");
	auto computeCommandBuffer = device.CreateCommandBuffer({
		.Usage = HE::Rendering::CommandBufferUsage::Compute,
		.DebugName = "RHICommandListBindingSmoke compute queue"
	});
	Require(static_cast<bool>(computeCommandBuffer), "Expected compute command buffer creation to succeed");
	Require(computeCommandBuffer->Begin() && computeCommandBuffer->End(), "Expected empty compute command buffer recording to succeed");
	const auto computeSubmit = device.GetComputeQueue().Submit({
		.CommandBufferPtr = computeCommandBuffer.get(),
		.WaitFence = secondSubmit.SignalFence,
		.WaitValue = secondSubmit.SignalValue
	});
	Require(computeSubmit, "Expected compute queue submit after graphics timeline wait to succeed");
	for (uint32_t poll = 0; poll < 100000 && computeSubmit.SignalFence->GetCompletedValue() < computeSubmit.SignalValue; ++poll) {}
	Require(computeSubmit.SignalFence->GetCompletedValue() == computeSubmit.SignalValue, "Expected compute GPU fence completion after polling");
	Require(!device.GetGraphicsQueue().Submit(*computeCommandBuffer), "Expected graphics queue to reject compute command buffer");
	auto copyCommandBuffer = device.CreateCommandBuffer({
		.Usage = HE::Rendering::CommandBufferUsage::Copy,
		.DebugName = "RHICommandListBindingSmoke copy queue"
	});
	Require(static_cast<bool>(copyCommandBuffer), "Expected copy command buffer creation to succeed");
	Require(copyCommandBuffer->Begin() && copyCommandBuffer->End(), "Expected empty copy command buffer recording to succeed");
	const auto copySubmit = device.GetCopyQueue().Submit({
		.CommandBufferPtr = copyCommandBuffer.get(),
		.WaitFence = computeSubmit.SignalFence,
		.WaitValue = computeSubmit.SignalValue
	});
	Require(copySubmit, "Expected copy queue submit after compute timeline wait to succeed");
	Require(!device.GetCopyQueue().Submit({ .CommandBufferPtr = copyCommandBuffer.get(), .WaitFence = computeSubmit.SignalFence, .WaitValue = computeSubmit.SignalValue + 1 }), "Expected unmet timeline wait to reject queue submit");
	HE::Rendering::DeferredReleaseQueue deferredReleaseQueue;
	ManualFence manualFence;
	auto retainedValue = std::make_shared<uint32_t>(42);
	std::weak_ptr<uint32_t> weakRetainedValue = retainedValue;
	deferredReleaseQueue.Track(retainedValue, &manualFence, 2);
	retainedValue.reset();
	deferredReleaseQueue.RetireCompleted();
	Require(!weakRetainedValue.expired() && deferredReleaseQueue.GetPendingCount() == 1, "Expected deferred release queue to retain resource before fence completion");
	manualFence.SetCompletedValue(2);
	deferredReleaseQueue.RetireCompleted();
	Require(weakRetainedValue.expired() && deferredReleaseQueue.GetPendingCount() == 0, "Expected deferred release queue to release resource after fence completion");
	VerifyRenderTargetSamples(renderTarget);
	recordedCommandBuffer->Reset();
	Require(!recordedCommandBuffer->IsExecutable(), "Expected reset recorded command buffer to clear executable state");

	std::cout << "RHICommandListBindingSmoke passed" << std::endl;
	return 0;
}
