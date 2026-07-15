#pragma once

#include <string>
#include <vector>

#include "HuaEngine/Rendering/RenderPipeline/RenderTypes.h"

namespace HE {
	class AssetResolver;
}

namespace HE::Rendering {
	class BindGroupLayout;
	struct MaterialBindingSchema;
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
			std::string MaterialSchemaSignature;
			Ref<BindGroupLayout> MaterialLayout;
			Ref<PipelineState> PipelineState;
		};

		struct MaterialBindGroupLayoutCacheEntry {
			std::string SchemaSignature;
			Ref<BindGroupLayout> Layout;
		};

		Ref<BindGroupLayout> GetFrameBindGroupLayout(RenderDevice& device, RenderStats& stats) const;
		Ref<BindGroupLayout> GetObjectBindGroupLayout(RenderDevice& device, RenderStats& stats) const;
		Ref<BindGroupLayout> GetMaterialBindGroupLayout(
			RenderDevice& device,
			const MaterialBindingSchema& schema,
			RenderStats& stats) const;
		Ref<PipelineState> GetPipelineState(
			RenderDevice& device,
			Ref<ShaderProgram> shaderProgram,
			const BufferLayout& vertexLayout,
			const std::string& materialSchemaSignature,
			Ref<BindGroupLayout> frameBindGroupLayout,
			Ref<BindGroupLayout> materialBindGroupLayout,
			Ref<BindGroupLayout> objectBindGroupLayout,
			RenderStats& stats) const;

		HE::AssetResolver* m_AssetResolver = nullptr;
		mutable Ref<BindGroupLayout> m_FrameBindGroupLayoutCache;
		mutable Ref<BindGroupLayout> m_ObjectBindGroupLayoutCache;
		mutable std::vector<MaterialBindGroupLayoutCacheEntry> m_MaterialBindGroupLayoutCache;
		mutable std::vector<PipelineStateCacheEntry> m_PipelineStateCache;
	};
}
