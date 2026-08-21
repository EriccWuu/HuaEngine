#include "enginepch.h"
#include "RenderResourceResolver.h"

#include "HuaEngine/Asset/AssetResolver.h"
#include "HuaEngine/Asset/AssetTypes.h"
#include "HuaEngine/Rendering/RenderPipeline/RenderBindGroupBuilder.h"
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

	RenderResourceResolver::RenderResourceResolver(HE::AssetResolver& assetResolver)
		: m_AssetResolver(&assetResolver) {}

	Ref<BindGroupLayout> RenderResourceResolver::GetFrameBindGroupLayout(RenderDevice& device, RenderStats& stats) const {
		if (m_FrameBindGroupLayoutCache) {
			++stats.BindGroupLayoutCacheHits;
			return m_FrameBindGroupLayoutCache;
		}

		++stats.BindGroupLayoutCacheMisses;
		m_FrameBindGroupLayoutCache = CreateFrameBindGroupLayout(device);
		return m_FrameBindGroupLayoutCache;
	}

	Ref<BindGroupLayout> RenderResourceResolver::GetObjectBindGroupLayout(RenderDevice& device, RenderStats& stats) const {
		if (m_ObjectBindGroupLayoutCache) {
			++stats.BindGroupLayoutCacheHits;
			return m_ObjectBindGroupLayoutCache;
		}

		++stats.BindGroupLayoutCacheMisses;
		m_ObjectBindGroupLayoutCache = CreateObjectBindGroupLayout(device);
		return m_ObjectBindGroupLayoutCache;
	}

	Ref<BindGroupLayout> RenderResourceResolver::GetMaterialBindGroupLayout(
		RenderDevice& device,
		const MaterialBindingSchema& schema,
		RenderStats& stats) const {
		if (schema.Signature.empty()) {
			return nullptr;
		}

		for (const auto& entry : m_MaterialBindGroupLayoutCache) {
			if (entry.SchemaSignature == schema.Signature) {
				++stats.BindGroupLayoutCacheHits;
				return entry.Layout;
			}
		}

		++stats.BindGroupLayoutCacheMisses;
		auto layout = CreateMaterialBindGroupLayout(device, schema);
		if (layout) {
			m_MaterialBindGroupLayoutCache.push_back({
				.SchemaSignature = schema.Signature,
				.Layout = layout
			});
		}

		return layout;
	}

	Ref<PipelineState> RenderResourceResolver::GetPipelineState(
		RenderDevice& device,
		Ref<ShaderProgram> shaderProgram,
		const BufferLayout& vertexLayout,
		const std::string& materialSchemaSignature,
		Ref<BindGroupLayout> frameBindGroupLayout,
		Ref<BindGroupLayout> materialBindGroupLayout,
		Ref<BindGroupLayout> objectBindGroupLayout,
		RenderStats& stats) const {
		if (!shaderProgram || materialSchemaSignature.empty() || !frameBindGroupLayout || !materialBindGroupLayout || !objectBindGroupLayout) {
			return nullptr;
		}

		for (const auto& entry : m_PipelineStateCache) {
			if (entry.Shader == shaderProgram
				&& BufferLayoutsMatch(entry.VertexLayout, vertexLayout)
				&& entry.MaterialSchemaSignature == materialSchemaSignature) {
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
				.MaterialSchemaSignature = materialSchemaSignature,
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
		auto frameBindGroupLayout = GetFrameBindGroupLayout(device, stats);
		auto objectBindGroupLayout = GetObjectBindGroupLayout(device, stats);
		const auto materialBindingSchema = baseMaterial->GetBindingSchema();
		auto materialBindGroupLayout = GetMaterialBindGroupLayout(device, materialBindingSchema, stats);

		outResolvedItem.MaterialInstanceRef = materialInstance;
		outResolvedItem.MaterialBindGroupRef = CreateMaterialBindGroup(device, *materialInstance, materialBindGroupLayout);
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
			materialBindingSchema.Signature,
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
