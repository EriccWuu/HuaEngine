#pragma once

#include <vector>

#include "HuaEngine/Rendering/RenderPipeline/RenderTypes.h"

namespace HE {
	class AssetResolver;
}

namespace HE::Rendering {
	class BindGroupLayout;
	class PipelineState;
	class RenderDevice;
	class ShaderProgram;

	class RenderResourceResolver {
	public:
		RenderResourceResolver() = default;
		explicit RenderResourceResolver(HE::AssetResolver& assetResolver);

		void SetAssetResolver(HE::AssetResolver* assetResolver) { m_AssetResolver = assetResolver; }

		bool Resolve(
			const RenderItem& item,
			ResolvedRenderItem& outResolvedItem,
			RenderStats& stats,
			std::vector<RenderDiagnostic>& diagnostics) const;

	private:
		struct PipelineStateCacheEntry {
			Ref<ShaderProgram> Shader;
			BufferLayout VertexLayout;
			std::vector<BindGroupLayoutEntry> MaterialLayoutEntries;
			Ref<BindGroupLayout> MaterialLayout;
			Ref<PipelineState> PipelineState;
		};

		Ref<BindGroupLayout> GetFrameBindGroupLayout(RenderDevice& device, RenderStats& stats) const;
		Ref<BindGroupLayout> GetObjectBindGroupLayout(RenderDevice& device, RenderStats& stats) const;
		Ref<PipelineState> GetPipelineState(
			RenderDevice& device,
			Ref<ShaderProgram> shaderProgram,
			const BufferLayout& vertexLayout,
			Ref<BindGroupLayout> frameBindGroupLayout,
			Ref<BindGroupLayout> materialBindGroupLayout,
			Ref<BindGroupLayout> objectBindGroupLayout,
			RenderStats& stats) const;

		HE::AssetResolver* m_AssetResolver = nullptr;
		mutable Ref<BindGroupLayout> m_FrameBindGroupLayoutCache;
		mutable Ref<BindGroupLayout> m_ObjectBindGroupLayoutCache;
		mutable std::vector<PipelineStateCacheEntry> m_PipelineStateCache;
	};
}
