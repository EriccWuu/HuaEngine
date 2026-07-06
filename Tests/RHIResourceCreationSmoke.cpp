#include <cstdlib>
#include <iostream>
#include <string>

#include "HuaEngine.h"
#include "HuaEngine/Core/ResourcePaths.h"
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

	HE::Rendering::FrameBufferSpecification frameBufferSpec;
	frameBufferSpec.Width = 64;
	frameBufferSpec.Height = 64;
	frameBufferSpec.Attachments = { HE::Rendering::FrameBufferTextureFormat::RGBA8, HE::Rendering::FrameBufferTextureFormat::DEPTH24_STENCIL8 };
	auto renderTarget = device.CreateRenderTarget({ .Specification = frameBufferSpec });
	Require(static_cast<bool>(renderTarget), "Expected render target creation to succeed");
	Require(renderTarget->GetSpecification().Width == 64, "Expected render target width");
	Require(renderTarget->GetSpecification().Height == 64, "Expected render target height");

	const auto texturePath = HE::ResourcePaths::ResolveEngineResourcePath("ret.png");
	auto texture = device.CreateTexture({ .SourcePath = texturePath.generic_string() });
	Require(static_cast<bool>(texture), "Expected texture resource creation to succeed");
	Require(texture->GetWidth() > 0 && texture->GetHeight() > 0, "Expected texture dimensions");

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

	HE::Rendering::GpuBufferDesc invalidBufferDesc;
	invalidBufferDesc.Usage = HE::Rendering::GpuBufferUsage::Vertex;
	invalidBufferDesc.Size = 0;
	Require(!device.CreateBuffer(invalidBufferDesc, nullptr), "Expected zero-sized GPU buffer creation to fail");

	std::cout << "RHIResourceCreationSmoke passed" << std::endl;
	return 0;
}
