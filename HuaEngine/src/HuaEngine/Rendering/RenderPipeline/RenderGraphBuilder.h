#pragma once

#include <functional>
#include <string>

#include "HuaEngine/Rendering/RenderPipeline/PassGraph.h"

namespace HE::Rendering {
	class RenderGraphPassBuilder {
	public:
		void DependsOn(PassGraphPassHandle pass);
		void Read(RenderGraphResourceHandle resource, ResourceState state);
		void Write(RenderGraphResourceHandle resource, ResourceState state);
		void WriteColor(
			RenderGraphResourceHandle resource,
			LoadOp load = LoadOp::Clear,
			StoreOp store = StoreOp::Store,
			const glm::vec4& clearColor = glm::vec4(0.0f));
		void WriteDepth(
			RenderGraphResourceHandle resource,
			LoadOp load = LoadOp::Clear,
			StoreOp store = StoreOp::Store,
			float clearDepth = 1.0f,
			uint32_t clearStencil = 0);
		void SetExecute(std::function<void(RenderPassContext&)> execute);

	private:
		friend class RenderGraphBuilder;

		explicit RenderGraphPassBuilder(PassGraphPassDesc& pass) : m_Pass(pass) {}

		PassGraphPassDesc& m_Pass;
	};

	class RenderGraphBuilder {
	public:
		explicit RenderGraphBuilder(PassGraph& graph);

		RenderGraphResourceHandle ImportTexture(std::string name, const Ref<TextureResource>& texture);
		RenderGraphResourceHandle ImportBuffer(std::string name, const Ref<GpuBuffer>& buffer);
		RenderGraphResourceHandle CreateTexture(std::string name, RenderGraphTextureDesc desc);
		RenderGraphResourceHandle CreateBuffer(std::string name, RenderGraphBufferDesc desc);

		PassGraphPassHandle AddGraphicsPass(const std::string& name, const std::function<void(RenderGraphPassBuilder&)>& setup);
		PassGraphPassHandle AddComputePass(const std::string& name, const std::function<void(RenderGraphPassBuilder&)>& setup);
		PassGraphPassHandle AddCopyPass(const std::string& name, const std::function<void(RenderGraphPassBuilder&)>& setup);
		void Export(RenderGraphResourceHandle resource);

	private:
		PassGraphPassHandle AddPass(
			const std::string& name,
			PassGraphPassType type,
			const std::function<void(RenderGraphPassBuilder&)>& setup);

		PassGraph& m_Graph;
	};
}
