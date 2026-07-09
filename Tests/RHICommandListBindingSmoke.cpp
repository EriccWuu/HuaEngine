#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

#include "HuaEngine.h"
#include "HuaEngine/Rendering/Material/MaterialBinding.h"
#include "HuaEngine/Rendering/Camera.h"
#include "HuaEngine/Rendering/RHI/CommandList.h"
#include "HuaEngine/Rendering/RHI/FrameObjectBinding.h"
#include "HuaEngine/Rendering/RHI/RenderHardwareInterface.h"

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

	HE::Rendering::MaterialBinding MakeColorBinding(const glm::vec4& color) {
		HE::Rendering::MaterialBinding materialBinding;
		materialBinding.Parameters.push_back({
			.Name = "u_Color",
			.Type = HE::Rendering::MaterialParameterType::Vec4,
			.Value = color
		});
		return materialBinding;
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
		uniform mat4 u_ViewProjection;
		uniform mat4 u_Transform;
		void main() {
			gl_Position = u_ViewProjection * u_Transform * vec4(a_Position, 1.0);
		}
	)";

	const std::string fragmentSource = R"(
		#version 330 core
		layout(location = 0) out vec4 color;
		uniform vec4 u_Color;
		void main() {
			color = u_Color;
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

	HE::Rendering::EditorCamera camera;
	camera.SetViewport(64.0f, 64.0f);

	auto& commands = device.GetImmediateCommandList();
	commands.BeginRenderTarget(*renderTarget);
	commands.ClearColor({ 0.1f, 0.1f, 0.1f, 1.0f });
	commands.BeginFrame(camera);
	commands.SetPipelineState(*pipelineState);
	commands.SetFrameBinding({ .ViewProjection = camera.GetViewProjection() });
	commands.SetVertexBufferView(*vertexBufferView);
	auto materialBinding = MakeColorBinding(glm::vec4(0.9f, 0.2f, 0.1f, 1.0f));
	commands.SetMaterialBinding(materialBinding);
	commands.SetObjectBinding({ .Transform = glm::mat4(1.0f) });
	commands.DrawIndexed(vertexBufferView->GetDesc().IndexCount);
	commands.EndFrame();
	commands.EndRenderTarget();
	VerifyRenderTargetSamples(renderTarget);

	commands.BeginRenderTarget(*renderTarget);
	commands.ClearColor({ 0.1f, 0.1f, 0.1f, 1.0f });
	commands.BeginFrame(camera);
	commands.SetFrameBinding({ .ViewProjection = camera.GetViewProjection() });
	commands.SetObjectBinding({ .Transform = glm::mat4(1.0f) });
	commands.SetPipelineState(*pipelineState);
	commands.SetVertexBufferView(*vertexBufferView);
	commands.SetMaterialBinding(materialBinding);
	commands.DrawIndexed(vertexBufferView->GetDesc().IndexCount);
	commands.EndFrame();
	commands.EndRenderTarget();
	VerifyRenderTargetSamples(renderTarget);

	std::cout << "RHICommandListBindingSmoke passed" << std::endl;
	return 0;
}
