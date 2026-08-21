#include "enginepch.h"
#include "EditorSceneRenderExtension.h"

#include "HuaEngine/Rendering/RenderPipeline/RenderBindGroupBuilder.h"
#include "HuaEngine/Rendering/RHI/CommandSubmission.h"
#include "HuaEngine/Rendering/RHI/CommandList.h"
#include "HuaEngine/Rendering/RHI/RenderDevice.h"

namespace HE::Editor {
	void EditorGridPass::Configure(
		Rendering::RenderGraphResourceHandle sceneColor,
		Rendering::RenderGraphResourceHandle sceneDepth,
		const glm::vec4& clearColor) {
		m_SceneColor = sceneColor;
		m_SceneDepth = sceneDepth;
		m_ClearColor = clearColor;
	}

	void EditorGridPass::Setup(Rendering::RenderGraphPassBuilder& builder) {
		builder.WriteColor(m_SceneColor, Rendering::LoadOp::Clear, Rendering::StoreOp::Store, m_ClearColor);
		builder.WriteDepth(m_SceneDepth, Rendering::LoadOp::Clear);
	}

	void EditorGridPass::Execute(Rendering::RenderPassContext& context) {
		if (!context.View || !context.View->CameraRef || !context.View->Target || !context.Device || !context.Commands || !context.Stats) return;

		std::vector<glm::vec3> vertices;
		std::vector<uint32_t> indices;
		constexpr int extent = 128;
		vertices.reserve((extent * 2 + 1) * 4);
		indices.reserve((extent * 2 + 1) * 4);
		for (int coordinate = -extent; coordinate <= extent; ++coordinate) {
			const uint32_t base = static_cast<uint32_t>(vertices.size());
			vertices.emplace_back(static_cast<float>(coordinate), 0.0f, static_cast<float>(-extent));
			vertices.emplace_back(static_cast<float>(coordinate), 0.0f, static_cast<float>(extent));
			vertices.emplace_back(static_cast<float>(-extent), 0.0f, static_cast<float>(coordinate));
			vertices.emplace_back(static_cast<float>(extent), 0.0f, static_cast<float>(coordinate));
			indices.insert(indices.end(), { base, base + 1, base + 2, base + 3 });
		}

		auto vertexBuffer = context.Device->CreateBuffer({ .Usage = Rendering::GpuBufferUsage::Vertex, .Size = static_cast<uint32_t>(vertices.size() * sizeof(glm::vec3)), .Stride = sizeof(glm::vec3) }, vertices.data());
		auto indexBuffer = context.Device->CreateBuffer({ .Usage = Rendering::GpuBufferUsage::Index, .Size = static_cast<uint32_t>(indices.size() * sizeof(uint32_t)), .Stride = sizeof(uint32_t) }, indices.data());
		auto shader = context.Device->CreateShaderProgram({
			.VertexSource = "#version 330 core\nlayout(location=0) in vec3 a_Position; uniform mat4 u_ViewProjection; void main(){ gl_Position=u_ViewProjection*vec4(a_Position,1.0); }",
			.FragmentSource = "#version 330 core\nlayout(location=0) out vec4 color; void main(){ color=vec4(0.25,0.29,0.35,1.0); }"
		});
		auto frameLayout = Rendering::CreateFrameBindGroupLayout(*context.Device);
		auto frameBindGroup = Rendering::CreateFrameBindGroup(*context.Device, context.View->CameraRef->GetViewProjection());
		if (!vertexBuffer || !indexBuffer || !shader || !frameLayout || !frameBindGroup) return;
		auto pipeline = context.Device->CreatePipelineState({
			.Shader = shader, .VertexLayout = { { Rendering::ShaderDataType::Float3, "a_Position" } }, .Topology = Rendering::PrimitiveTopology::LineList,
			.ColorTargets = { { .Format = context.View->Target->GetColorAttachmentTexture()->GetDesc().Format } },
			.DepthStencil = { .Format = Rendering::RenderTargetTextureFormat::DEPTH24_STENCIL8, .DepthTestEnabled = true, .DepthWriteEnabled = false },
			.Raster = { .Cull = Rendering::CullMode::None }, .BindGroupLayouts = { { .Slot = 0, .Layout = frameLayout } }
		});
		if (!pipeline) return;
		if (context.RecordingCommandBuffer) {
			context.RecordingCommandBuffer->RetainResource(vertexBuffer); context.RecordingCommandBuffer->RetainResource(indexBuffer);
			context.RecordingCommandBuffer->RetainResource(shader); context.RecordingCommandBuffer->RetainResource(frameLayout);
			context.RecordingCommandBuffer->RetainResource(frameBindGroup); context.RecordingCommandBuffer->RetainResource(pipeline);
		}
		context.Commands->SetPipelineState(*pipeline);
		context.Commands->SetVertexBuffer(0, { .Buffer = vertexBuffer, .Stride = sizeof(glm::vec3) });
		context.Commands->SetIndexBuffer({ .Buffer = indexBuffer, .Format = Rendering::IndexFormat::UInt32, .IndexCount = static_cast<uint32_t>(indices.size()) });
		context.Commands->SetBindGroup(0, *frameBindGroup);
		context.Commands->DrawIndexed(static_cast<uint32_t>(indices.size()));
		++context.Stats->DrawCalls; ++context.Stats->PassCount;
	}

	void EditorSceneRenderExtension::AddBeforeOpaquePasses(
		Rendering::RenderGraphBuilder& graph,
		const Rendering::ForwardSceneResources& resources,
		const Rendering::RenderView& view) {
		m_EditorGridPass.Configure(resources.Color, resources.Depth, view.ClearColor);
		graph.AddPass(m_EditorGridPass);
	}
}
