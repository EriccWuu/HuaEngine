#include "enginepch.h"
#include "RenderResourceResolver.h"

#include "HuaEngine/Asset/AssetResolver.h"
#include "HuaEngine/Asset/AssetTypes.h"
#include "HuaEngine/Rendering/RenderPipeline/RenderBindGroupBuilder.h"
#include "HuaEngine/Rendering/RenderPipeline/UniformBufferArena.h"
#include "HuaEngine/Rendering/RHI/RenderHardwareInterface.h"
#include "Module/Rendering/RenderingComponent.h"

namespace HE::Rendering {
	namespace {
		void AddDiagnostic(
			std::vector<RenderDiagnostic>& diagnostics,
			RenderDiagnosticCode code,
			Entity sourceEntity,
			std::string message) {
			diagnostics.push_back({ code, sourceEntity, std::move(message) });
		}

		void AddFallbackDiagnostic(
			std::vector<RenderDiagnostic>& diagnostics,
			Entity sourceEntity,
			const AssetGuid& requestedGuid,
			const AssetGuid& fallbackGuid) {
			AddDiagnostic(
				diagnostics,
				RenderDiagnosticCode::FallbackResourceUsed,
				sourceEntity,
				"Asset resolve failed for " + requestedGuid + "; using fallback " + fallbackGuid);
		}

		bool BufferLayoutsMatch(const BufferLayout& lhs, const BufferLayout& rhs) {
			if (lhs.GetStride() != rhs.GetStride() || lhs.GetElements().size() != rhs.GetElements().size()) {
				return false;
			}

			for (size_t index = 0; index < lhs.GetElements().size(); ++index) {
				const auto& left = lhs.GetElements()[index];
				const auto& right = rhs.GetElements()[index];
				if (left.Type != right.Type
					|| left.Name != right.Name
					|| left.Size != right.Size
					|| left.Offset != right.Offset
					|| left.Normalized != right.Normalized) {
					return false;
				}
			}

			return true;
		}

	}

	RenderResourceResolver::RenderResourceResolver() = default;

	RenderResourceResolver::RenderResourceResolver(HE::AssetResolver& assetResolver)
		: m_AssetResolver(&assetResolver) {}

	RenderResourceResolver::~RenderResourceResolver() = default;

	UniformBufferArena& RenderResourceResolver::GetUniformBufferArena(RenderDevice& device) const {
		if (!m_UniformBufferArena) m_UniformBufferArena = std::make_unique<UniformBufferArena>(device);
		return *m_UniformBufferArena;
	}

	Ref<BindGroupLayout> RenderResourceResolver::GetUniformBlockBindGroupLayout(RenderDevice& device, BindGroupScope scope, const ShaderUniformBlockBinding& block, RenderStats& stats) const {
		const std::string signature = std::to_string(static_cast<uint32_t>(scope)) + '#' + block.Name + '#' + std::to_string(block.BindingPoint) + '#' + std::to_string(block.Size);
		for (const auto& entry : m_BindGroupLayoutCache) {
			if (entry.InterfaceSignature == signature) {
				++stats.BindGroupLayoutCacheHits;
				return entry.Layout;
			}
		}
		++stats.BindGroupLayoutCacheMisses;
		auto layout = CreateUniformBlockBindGroupLayout(device, scope, block);
		if (layout) m_BindGroupLayoutCache.push_back({ signature, layout });
		return layout;
	}

	Ref<BindGroupLayout> RenderResourceResolver::GetMaterialBindGroupLayout(
		RenderDevice& device,
		const ShaderUniformBlockBinding& block,
		const std::vector<ShaderTextureBinding>& textures,
		RenderStats& stats) const {
		std::string interfaceSignature = std::to_string(static_cast<uint32_t>(BindGroupScope::Material)) + '#' + block.Name + '#' + std::to_string(block.BindingPoint) + '#' + std::to_string(block.Size);
		for (const auto& member : block.Members) interfaceSignature += member.Name + '#' + std::to_string(member.Offset) + '#' + std::to_string(member.Size) + ';';
		for (const auto& texture : textures) interfaceSignature += texture.TextureName + '#' + std::to_string(texture.TextureUnit) + ';';

		for (const auto& entry : m_BindGroupLayoutCache) {
			if (entry.InterfaceSignature == interfaceSignature) {
				++stats.BindGroupLayoutCacheHits;
				return entry.Layout;
			}
		}

		++stats.BindGroupLayoutCacheMisses;
		auto layout = CreateMaterialBindGroupLayout(device, block, textures);
		if (layout) {
			m_BindGroupLayoutCache.push_back({
				.InterfaceSignature = std::move(interfaceSignature),
				.Layout = layout
			});
		}

		return layout;
	}

	Ref<PipelineState> RenderResourceResolver::GetPipelineState(
		RenderDevice& device,
		Ref<ShaderProgram> shaderProgram,
		const BufferLayout& vertexLayout,
		uint64_t interfaceSignature,
		Ref<BindGroupLayout> frameBindGroupLayout,
		Ref<BindGroupLayout> materialBindGroupLayout,
		Ref<BindGroupLayout> objectBindGroupLayout,
		RenderStats& stats) const {
		if (!shaderProgram || interfaceSignature == 0 || !frameBindGroupLayout || !materialBindGroupLayout || !objectBindGroupLayout) {
			return nullptr;
		}

		for (const auto& entry : m_PipelineStateCache) {
			if (entry.Shader == shaderProgram
				&& BufferLayoutsMatch(entry.VertexLayout, vertexLayout)
				&& entry.InterfaceSignature == interfaceSignature) {
				++stats.PipelineStateCacheHits;
				return entry.PipelineState;
			}
		}

		++stats.PipelineStateCacheMisses;
		auto pipelineState = device.CreatePipelineState({
			.Shader = shaderProgram,
			.VertexLayout = vertexLayout,
			.Topology = PrimitiveTopology::TriangleList,
			.BindGroupLayouts = {
				{
					.Slot = 0,
					.Layout = frameBindGroupLayout
				},
				{
					.Slot = 1,
					.Layout = materialBindGroupLayout
				},
				{
					.Slot = 2,
					.Layout = objectBindGroupLayout
				}
			}
		});
		if (pipelineState) {
			m_PipelineStateCache.push_back({
				.Shader = shaderProgram,
				.VertexLayout = vertexLayout,
				.InterfaceSignature = interfaceSignature,
				.MaterialLayout = materialBindGroupLayout,
				.PipelineState = pipelineState
			});
		}

		return pipelineState;
	}

	bool RenderResourceResolver::Resolve(
		const RenderItem& item,
		ResolvedRenderItem& outResolvedItem,
		RenderStats& stats,
		std::vector<RenderDiagnostic>& diagnostics) const {
		outResolvedItem = {};
		outResolvedItem.Source = &item;

		if (!m_AssetResolver) {
			AddDiagnostic(
				diagnostics,
				RenderDiagnosticCode::MissingMeshAsset,
				item.SourceEntity,
				"Render resource resolver has no asset resolver");
			return false;
		}

		Ref<Mesh> mesh = nullptr;
		bool usedFallback = false;
		const AssetGuid requestedMeshGuid = item.Mesh.Reference.Guid;
		auto meshResult = m_AssetResolver->ResolveMesh(requestedMeshGuid, mesh);
		if (!meshResult.Succeeded() || !mesh || !mesh->GetVertexBufferView()) {
			Ref<Mesh> fallbackMesh = nullptr;
			const auto fallbackResult = m_AssetResolver->ResolveMesh(BuiltinAssetGuids::FallbackMesh, fallbackMesh);
			if (!fallbackResult.Succeeded() || !fallbackMesh || !fallbackMesh->GetVertexBufferView()) {
				AddDiagnostic(
					diagnostics,
					RenderDiagnosticCode::MissingMeshAsset,
					item.SourceEntity,
					"Fallback mesh asset is unavailable: " + BuiltinAssetGuids::FallbackMesh);
				return false;
			}

			mesh = fallbackMesh;
			usedFallback = true;
			AddFallbackDiagnostic(diagnostics, item.SourceEntity, requestedMeshGuid, BuiltinAssetGuids::FallbackMesh);
		}

		Ref<Material> baseMaterial = nullptr;
		const AssetGuid requestedMaterialGuid = item.Material.Reference.Guid;
		auto materialResult = m_AssetResolver->ResolveMaterial(requestedMaterialGuid, baseMaterial);
		if (!materialResult.Succeeded() || !baseMaterial || !baseMaterial->GetShaderProgram()) {
			Ref<Material> fallbackMaterial = nullptr;
			const auto fallbackResult = m_AssetResolver->ResolveMaterial(BuiltinAssetGuids::FallbackMaterial, fallbackMaterial);
			if (!fallbackResult.Succeeded() || !fallbackMaterial || !fallbackMaterial->GetShaderProgram()) {
				AddDiagnostic(
					diagnostics,
					RenderDiagnosticCode::MissingBaseMaterial,
					item.SourceEntity,
					"Fallback material asset is unavailable: " + BuiltinAssetGuids::FallbackMaterial);
				return false;
			}

			baseMaterial = fallbackMaterial;
			usedFallback = true;
			AddFallbackDiagnostic(diagnostics, item.SourceEntity, requestedMaterialGuid, BuiltinAssetGuids::FallbackMaterial);
		}

		Ref<MaterialInstance> materialInstance = baseMaterial->CreateInstance();
		if (!materialInstance) {
			AddDiagnostic(
				diagnostics,
				RenderDiagnosticCode::MissingMaterialInstance,
				item.SourceEntity,
				"Render item material instance could not be created");
			return false;
		}

		for (const auto& [parameterName, value] : item.MaterialOverrides.Parameters) {
			if (baseMaterial->HasParameter(parameterName)) {
				materialInstance->SetParameter(parameterName, value);
			}
		}

		if (usedFallback) {
			++stats.FallbackItems;
		}

		auto& device = RenderHardwareInterface::GetDevice();
		const auto& shaderDesc = baseMaterial->GetShaderProgram()->GetDesc();
		const auto frameBlock = std::find_if(shaderDesc.UniformBlocks.begin(), shaderDesc.UniformBlocks.end(), [](const auto& block) { return block.Set == 0; });
		const auto materialBlock = std::find_if(shaderDesc.UniformBlocks.begin(), shaderDesc.UniformBlocks.end(), [](const auto& block) { return block.Set == 1; });
		const auto objectBlock = std::find_if(shaderDesc.UniformBlocks.begin(), shaderDesc.UniformBlocks.end(), [](const auto& block) { return block.Set == 2; });
		if (frameBlock == shaderDesc.UniformBlocks.end() || materialBlock == shaderDesc.UniformBlocks.end() || objectBlock == shaderDesc.UniformBlocks.end()) {
			AddDiagnostic(diagnostics, RenderDiagnosticCode::MissingMaterialInstance, item.SourceEntity, "Shader must declare Frame, Material, and Object uniform blocks");
			return false;
		}
		auto frameBindGroupLayout = GetUniformBlockBindGroupLayout(device, BindGroupScope::Frame, *frameBlock, stats);
		auto objectBindGroupLayout = GetUniformBlockBindGroupLayout(device, BindGroupScope::Object, *objectBlock, stats);
		for (const auto& [parameterName, textureGuid] : item.MaterialOverrides.TextureParameters) {
			if (!baseMaterial->HasParameter(parameterName)) continue;
			Ref<TextureResource> texture;
			if (m_AssetResolver->ResolveTexture(textureGuid, texture).Succeeded() && texture) materialInstance->SetParameter(parameterName, texture);
		}
		auto materialBindGroupLayout = GetMaterialBindGroupLayout(device, *materialBlock, shaderDesc.Textures, stats);

		outResolvedItem.MaterialInstanceRef = materialInstance;
		outResolvedItem.FrameBlock = *frameBlock;
		outResolvedItem.ObjectBlock = *objectBlock;
		outResolvedItem.FrameBindGroupLayoutRef = frameBindGroupLayout;
		outResolvedItem.ObjectBindGroupLayoutRef = objectBindGroupLayout;
		outResolvedItem.MaterialBindGroupRef = CreateMaterialBindGroup(device, GetUniformBufferArena(device), *materialInstance, *materialBlock, shaderDesc.Textures, materialBindGroupLayout);
		if (!outResolvedItem.MaterialBindGroupRef || !outResolvedItem.MaterialBindGroupRef->GetDesc().Layout) {
			AddDiagnostic(
				diagnostics,
				RenderDiagnosticCode::MissingMaterialInstance,
				item.SourceEntity,
				"Render item material bind group could not be created");
			return false;
		}

		outResolvedItem.VertexBufferViewRef = mesh->GetVertexBufferView();
		const auto& vertexBufferViewDesc = outResolvedItem.VertexBufferViewRef->GetDesc();
		outResolvedItem.VertexBinding = {
			.Buffer = vertexBufferViewDesc.VertexBuffer,
			.Offset = 0,
			.Stride = vertexBufferViewDesc.VertexBuffer ? vertexBufferViewDesc.VertexBuffer->GetDesc().Stride : 0
		};
		outResolvedItem.IndexBinding = {
			.Buffer = vertexBufferViewDesc.IndexBuffer,
			.Offset = 0,
			.Format = vertexBufferViewDesc.IndexFormatValue,
			.IndexCount = vertexBufferViewDesc.IndexCount
		};
		outResolvedItem.ShaderProgramRef = baseMaterial->GetShaderProgram();
		outResolvedItem.PipelineStateRef = GetPipelineState(
			device,
			outResolvedItem.ShaderProgramRef,
			outResolvedItem.VertexBufferViewRef->GetDesc().Layout,
			shaderDesc.InterfaceSignature,
			frameBindGroupLayout,
			outResolvedItem.MaterialBindGroupRef->GetDesc().Layout,
			objectBindGroupLayout,
			stats);
		if (!outResolvedItem.PipelineStateRef) {
			AddDiagnostic(
				diagnostics,
				RenderDiagnosticCode::MissingPipelineState,
				item.SourceEntity,
				"Render item pipeline state could not be created");
			return false;
		}

		return true;
	}
}
