#include "enginepch.h"
#include "RenderGraphBuilder.h"

namespace HE::Rendering {
	void RenderGraphPassBuilder::DependsOn(PassGraphPassHandle pass) {
		m_Pass.Dependencies.push_back(pass);
	}

	void RenderGraphPassBuilder::Read(RenderGraphResourceHandle resource, ResourceState state) {
		m_Pass.ResourceUsages.push_back({
			.Resource = resource,
			.AccessMode = PassGraphResourceUsage::Access::Read,
			.State = state
		});
	}

	void RenderGraphPassBuilder::Write(RenderGraphResourceHandle resource, ResourceState state) {
		m_Pass.ResourceUsages.push_back({
			.Resource = resource,
			.AccessMode = PassGraphResourceUsage::Access::Write,
			.State = state
		});
	}

	void RenderGraphPassBuilder::WriteColor(
		RenderGraphResourceHandle resource,
		LoadOp load,
		StoreOp store,
		const glm::vec4& clearColor) {
		m_Pass.RenderPassAttachments.push_back({
			.Resource = resource,
			.Kind = PassGraphRenderPassAttachmentKind::Color,
			.Load = load,
			.Store = store,
			.ClearColor = clearColor
		});
	}

	void RenderGraphPassBuilder::WriteDepth(
		RenderGraphResourceHandle resource,
		LoadOp load,
		StoreOp store,
		float clearDepth,
		uint32_t clearStencil) {
		m_Pass.RenderPassAttachments.push_back({
			.Resource = resource,
			.Kind = PassGraphRenderPassAttachmentKind::DepthStencil,
			.Load = load,
			.Store = store,
			.ClearDepth = clearDepth,
			.ClearStencil = clearStencil
		});
	}

	void RenderGraphPassBuilder::SetExecute(std::function<void(RenderPassContext&)> execute) {
		m_Pass.Execute = std::move(execute);
	}

	RenderGraphBuilder::RenderGraphBuilder(PassGraph& graph) : m_Graph(graph) {
		m_Graph.Reset();
	}

	RenderGraphResourceHandle RenderGraphBuilder::ImportTexture(std::string name, const Ref<TextureResource>& texture) {
		RenderGraphResourceDesc desc;
		desc.Name = std::move(name);
		desc.Kind = RenderGraphResourceKind::Texture;
		desc.Texture = {
			.Width = texture ? texture->GetWidth() : 0,
			.Height = texture ? texture->GetHeight() : 0,
			.Format = texture ? texture->GetDesc().Format : RenderTargetTextureFormat::None
		};
		desc.RuntimeTexture = texture;
		return m_Graph.AddImportedResource(std::move(desc));
	}

	RenderGraphResourceHandle RenderGraphBuilder::ImportBuffer(std::string name, const Ref<GpuBuffer>& buffer) {
		RenderGraphResourceDesc desc;
		desc.Name = std::move(name);
		desc.Kind = RenderGraphResourceKind::Buffer;
		desc.Buffer = buffer ? RenderGraphBufferDesc{
			.Size = buffer->GetDesc().Size,
			.Stride = buffer->GetDesc().Stride,
			.Usage = buffer->GetDesc().Usage
		} : RenderGraphBufferDesc{};
		desc.RuntimeBuffer = buffer;
		return m_Graph.AddImportedResource(std::move(desc));
	}

	RenderGraphResourceHandle RenderGraphBuilder::CreateTexture(std::string name, RenderGraphTextureDesc desc) {
		return m_Graph.AddTransientResource({
			.Name = std::move(name),
			.Kind = RenderGraphResourceKind::Texture,
			.Texture = std::move(desc)
		});
	}

	RenderGraphResourceHandle RenderGraphBuilder::CreateBuffer(std::string name, RenderGraphBufferDesc desc) {
		return m_Graph.AddTransientResource({
			.Name = std::move(name),
			.Kind = RenderGraphResourceKind::Buffer,
			.Buffer = desc
		});
	}

	PassGraphPassHandle RenderGraphBuilder::AddGraphicsPass(
		const std::string& name,
		const std::function<void(RenderGraphPassBuilder&)>& setup) {
		return AddPass(name, PassGraphPassType::Graphics, setup);
	}

	PassGraphPassHandle RenderGraphBuilder::AddComputePass(
		const std::string& name,
		const std::function<void(RenderGraphPassBuilder&)>& setup) {
		return AddPass(name, PassGraphPassType::Compute, setup);
	}

	PassGraphPassHandle RenderGraphBuilder::AddCopyPass(
		const std::string& name,
		const std::function<void(RenderGraphPassBuilder&)>& setup) {
		return AddPass(name, PassGraphPassType::Copy, setup);
	}

	void RenderGraphBuilder::Export(RenderGraphResourceHandle resource) {
		m_Graph.AddOutputResource(resource);
	}

	PassGraphPassHandle RenderGraphBuilder::AddPass(
		const std::string& name,
		PassGraphPassType type,
		const std::function<void(RenderGraphPassBuilder&)>& setup) {
		PassGraphPassDesc pass;
		pass.Name = name;
		pass.Type = type;
		RenderGraphPassBuilder passBuilder(pass);
		setup(passBuilder);
		return m_Graph.AddPass(std::move(pass));
	}
}
