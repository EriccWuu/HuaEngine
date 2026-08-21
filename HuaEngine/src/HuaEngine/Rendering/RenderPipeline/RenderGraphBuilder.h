#pragma once

#include <functional>
#include <string>

#include "HuaEngine/Rendering/RenderPipeline/RenderGraph.h"

namespace HE::Rendering {
	class RenderGraphPassBuilder {
	public:
		void DependsOn(RenderGraphPassHandle pass);
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

		explicit RenderGraphPassBuilder(RenderGraphPassDesc& pass) : m_Pass(pass) {}

		RenderGraphPassDesc& m_Pass;
	};

	class RenderGraphPass {
	public:
		virtual ~RenderGraphPass() = default;

		[[nodiscard]] virtual const char* GetName() const = 0;
		[[nodiscard]] virtual RenderGraphPassType GetType() const = 0;
		virtual void Setup(RenderGraphPassBuilder& builder) = 0;
		virtual void Execute(RenderPassContext& context) = 0;
	};

	class RenderGraphBuilder {
	public:
		explicit RenderGraphBuilder(RenderGraph& graph);

		RenderGraphResourceHandle ImportTexture(std::string name, const Ref<TextureResource>& texture);
		RenderGraphResourceHandle ImportBuffer(std::string name, const Ref<GpuBuffer>& buffer);
		RenderGraphResourceHandle CreateTexture(std::string name, RenderGraphTextureDesc desc);
		RenderGraphResourceHandle CreateBuffer(std::string name, RenderGraphBufferDesc desc);

		RenderGraphPassHandle AddPass(
			const std::string& name,
			RenderGraphPassType type,
			const std::function<void(RenderGraphPassBuilder&)>& setup);
		RenderGraphPassHandle AddPass(RenderGraphPass& pass);
		RenderGraphPassHandle AddPass(const Ref<RenderGraphPass>& pass);
		void Export(RenderGraphResourceHandle resource);

	private:
		RenderGraph& m_Graph;
	};
}
