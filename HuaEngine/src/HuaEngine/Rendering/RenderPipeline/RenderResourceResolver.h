#pragma once

#include <string>
#include <memory>
#include <vector>

#include "HuaEngine/Rendering/RenderPipeline/RenderTypes.h"
#include "HuaEngine/Core/Sha256.h"
#include "HuaEngine/Rendering/Shader/ShaderInterface.h"

namespace HE {
	class AssetResolver;
}

namespace HE::Rendering {
	class BindGroupLayout;
	class PipelineState;
	class RenderDevice;
	class ShaderProgram;
	class UniformBufferArena;
	class RenderResourceResolver {
	public:
		RenderResourceResolver();
		explicit RenderResourceResolver(HE::AssetResolver& assetResolver);
		~RenderResourceResolver();

		void SetAssetResolver(HE::AssetResolver* assetResolver) { m_AssetResolver = assetResolver; }
		UniformBufferArena& GetUniformBufferArena(RenderDevice& device) const;

		bool Resolve(
			const RenderItem& item,
			ResolvedRenderItem& outResolvedItem,
			RenderStats& stats,
			std::vector<RenderDiagnostic>& diagnostics) const;

	private:
		struct PipelineStateCacheEntry {
			Ref<ShaderProgram> Shader;
			BufferLayout VertexLayout;
			uint64_t InterfaceSignature = 0;
			Ref<BindGroupLayout> MaterialLayout;
			Ref<PipelineState> PipelineState;
		};

		struct BindGroupLayoutCacheEntry {
			std::string InterfaceSignature;
			Ref<BindGroupLayout> Layout;
		};

		Ref<BindGroupLayout> GetUniformBlockBindGroupLayout(RenderDevice& device, BindGroupScope scope, const ShaderConstantBuffer& block, ShaderStageFlags visibility, const Sha256Digest& interfaceDigest, RenderStats& stats) const;
		Ref<BindGroupLayout> GetMaterialBindGroupLayout(
			RenderDevice& device,
			const ShaderConstantBuffer& block,
			ShaderStageFlags blockVisibility,
			const std::vector<ShaderResourceBinding>& textures,
			const Sha256Digest& interfaceDigest,
			RenderStats& stats) const;
		Ref<PipelineState> GetPipelineState(
			RenderDevice& device,
			Ref<ShaderProgram> shaderProgram,
			const BufferLayout& vertexLayout,
			uint64_t interfaceSignature,
			Ref<BindGroupLayout> frameBindGroupLayout,
			Ref<BindGroupLayout> materialBindGroupLayout,
			Ref<BindGroupLayout> objectBindGroupLayout,
			RenderStats& stats) const;

		HE::AssetResolver* m_AssetResolver = nullptr;
		mutable std::vector<BindGroupLayoutCacheEntry> m_BindGroupLayoutCache;
		mutable std::vector<PipelineStateCacheEntry> m_PipelineStateCache;
		mutable std::unique_ptr<UniformBufferArena> m_UniformBufferArena;
	};
}
