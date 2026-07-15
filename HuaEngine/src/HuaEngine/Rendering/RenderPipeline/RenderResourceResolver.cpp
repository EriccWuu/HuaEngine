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

	}

	RenderResourceResolver::RenderResourceResolver(HE::AssetResolver& assetResolver)
		: m_AssetResolver(&assetResolver) {}

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
			materialInstance->SetParameter(parameterName, value);
		}

		if (usedFallback) {
			++stats.FallbackItems;
		}

		auto& device = RenderHardwareInterface::GetDevice();
		auto frameBindGroupLayout = CreateFrameBindGroupLayout(device);
		auto objectBindGroupLayout = CreateObjectBindGroupLayout(device);

		outResolvedItem.MaterialInstanceRef = materialInstance;
		outResolvedItem.MaterialBindGroupRef = CreateMaterialBindGroup(device, *materialInstance);
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
		outResolvedItem.PipelineStateRef = device.CreatePipelineState({
			.Shader = outResolvedItem.ShaderProgramRef,
			.VertexLayout = outResolvedItem.VertexBufferViewRef->GetDesc().Layout,
			.Topology = PrimitiveTopology::TriangleList,
			.BindGroupLayouts = {
				{
					.Slot = 0,
					.Layout = frameBindGroupLayout
				},
				{
					.Slot = 1,
					.Layout = outResolvedItem.MaterialBindGroupRef->GetDesc().Layout
				},
				{
					.Slot = 2,
					.Layout = objectBindGroupLayout
				}
			}
		});
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
