#include "enginepch.h"
#include "PostProcessPass.h"

#include "HuaEngine/Rendering/RHI/CommandBufferRecorder.h"
#include "HuaEngine/Rendering/RHI/CommandList.h"
#include "HuaEngine/Rendering/RHI/RenderDevice.h"

namespace HE::Rendering {
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

}
