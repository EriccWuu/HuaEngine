#include "enginepch.h"
#include "EditorSceneRenderExtension.h"

#include "HuaEngine/Rendering/RenderPipeline/RenderBindGroupBuilder.h"
#include "HuaEngine/Rendering/RHI/CommandSubmission.h"
#include "HuaEngine/Rendering/RHI/CommandList.h"
#include "HuaEngine/Rendering/RHI/RenderDevice.h"

namespace HE::Editor {
	namespace {
		struct GridVertex {
			glm::vec3 Position;
			glm::vec3 Color;
		};
	}

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

		std::vector<GridVertex> vertices;
		std::vector<uint32_t> indices;
		constexpr int extent = 128;
		vertices.reserve((extent * 2 + 1) * 4);
		indices.reserve((extent * 2 + 1) * 4);
		for (int coordinate = -extent; coordinate <= extent; ++coordinate) {
			const uint32_t base = static_cast<uint32_t>(vertices.size());
			const glm::vec3 zAxisColor = coordinate == 0 ? glm::vec3(0.20f, 0.45f, 1.0f) : glm::vec3(0.25f, 0.29f, 0.35f);
			const glm::vec3 xAxisColor = coordinate == 0 ? glm::vec3(1.0f, 0.25f, 0.20f) : glm::vec3(0.25f, 0.29f, 0.35f);
			vertices.push_back({ { static_cast<float>(coordinate), 0.0f, static_cast<float>(-extent) }, zAxisColor });
			vertices.push_back({ { static_cast<float>(coordinate), 0.0f, static_cast<float>(extent) }, zAxisColor });
			vertices.push_back({ { static_cast<float>(-extent), 0.0f, static_cast<float>(coordinate) }, xAxisColor });
			vertices.push_back({ { static_cast<float>(extent), 0.0f, static_cast<float>(coordinate) }, xAxisColor });
			indices.insert(indices.end(), { base, base + 1, base + 2, base + 3 });
		}

		auto vertexBuffer = context.Device->CreateBuffer({ .Usage = Rendering::GpuBufferUsage::Vertex, .Size = static_cast<uint32_t>(vertices.size() * sizeof(GridVertex)), .Stride = sizeof(GridVertex) }, vertices.data());
		auto indexBuffer = context.Device->CreateBuffer({ .Usage = Rendering::GpuBufferUsage::Index, .Size = static_cast<uint32_t>(indices.size() * sizeof(uint32_t)), .Stride = sizeof(uint32_t) }, indices.data());
		auto shader = context.Device->CreateShaderProgram({
			.VertexSource = "#version 330 core\nlayout(location=0) in vec3 a_Position; layout(location=1) in vec3 a_Color; uniform mat4 u_ViewProjection; out vec3 v_Color; void main(){ gl_Position=u_ViewProjection*vec4(a_Position,1.0); v_Color=a_Color; }",
			.FragmentSource = "#version 330 core\nin vec3 v_Color; layout(location=0) out vec4 color; void main(){ color=vec4(v_Color,1.0); }"
		});
		auto frameLayout = Rendering::CreateFrameBindGroupLayout(*context.Device);
		auto frameBindGroup = Rendering::CreateFrameBindGroup(*context.Device, context.View->CameraRef->GetViewProjection());
		if (!vertexBuffer || !indexBuffer || !shader || !frameLayout || !frameBindGroup) return;
		auto pipeline = context.Device->CreatePipelineState({
			.Shader = shader, .VertexLayout = { { Rendering::ShaderDataType::Float3, "a_Position" }, { Rendering::ShaderDataType::Float3, "a_Color" } }, .Topology = Rendering::PrimitiveTopology::LineList,
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
		context.Commands->SetVertexBuffer(0, { .Buffer = vertexBuffer, .Stride = sizeof(GridVertex) });
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
