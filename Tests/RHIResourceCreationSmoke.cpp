#include <cstdlib>
#include <iostream>
#include <string>

#include "HuaEngine.h"
#include "HuaEngine/Core/ResourcePaths.h"
#include "HuaEngine/Rendering/RHI/CommandList.h"
#include "HuaEngine/Rendering/RHI/ResourceBarrier.h"
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
	device.GetImmediateCommandList().ResourceBarrier({
		.Texture = texture,
		.Before = HE::Rendering::ResourceState::Undefined,
		.After = HE::Rendering::ResourceState::ShaderRead
	});

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
