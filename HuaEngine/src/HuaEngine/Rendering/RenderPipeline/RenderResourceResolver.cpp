#include "enginepch.h"
#include "RenderResourceResolver.h"

#include "HuaEngine/Asset/AssetResolver.h"
#include "HuaEngine/Asset/AssetTypes.h"
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
		const AssetGuid requestedMeshGuid = item.Mesh.Reference.Guid;
		auto meshResult = m_AssetResolver->ResolveMesh(requestedMeshGuid, mesh);
		if (!meshResult.Succeeded() || !mesh || !mesh->GetVertexArray()) {
			Ref<Mesh> fallbackMesh = nullptr;
			const auto fallbackResult = m_AssetResolver->ResolveMesh(BuiltinAssetGuids::FallbackMesh, fallbackMesh);
			if (!fallbackResult.Succeeded() || !fallbackMesh || !fallbackMesh->GetVertexArray()) {
				AddDiagnostic(
					diagnostics,
					RenderDiagnosticCode::MissingMeshAsset,
					item.SourceEntity,
					"Fallback mesh asset is unavailable: " + BuiltinAssetGuids::FallbackMesh);
				return false;
			}

			mesh = fallbackMesh;
			++stats.FallbackItems;
			AddFallbackDiagnostic(diagnostics, item.SourceEntity, requestedMeshGuid, BuiltinAssetGuids::FallbackMesh);
		}

		Ref<Material> baseMaterial = nullptr;
		const AssetGuid requestedMaterialGuid = item.Material.Reference.Guid;
		auto materialResult = m_AssetResolver->ResolveMaterial(requestedMaterialGuid, baseMaterial);
		if (!materialResult.Succeeded() || !baseMaterial || !baseMaterial->GetShader()) {
			Ref<Material> fallbackMaterial = nullptr;
			const auto fallbackResult = m_AssetResolver->ResolveMaterial(BuiltinAssetGuids::FallbackMaterial, fallbackMaterial);
			if (!fallbackResult.Succeeded() || !fallbackMaterial || !fallbackMaterial->GetShader()) {
				AddDiagnostic(
					diagnostics,
					RenderDiagnosticCode::MissingBaseMaterial,
					item.SourceEntity,
					"Fallback material asset is unavailable: " + BuiltinAssetGuids::FallbackMaterial);
				return false;
			}

			baseMaterial = fallbackMaterial;
			++stats.FallbackItems;
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

		outResolvedItem.VertexArrayRef = mesh->GetVertexArray();
		outResolvedItem.MaterialInstanceRef = materialInstance;
		return true;
	}
}
