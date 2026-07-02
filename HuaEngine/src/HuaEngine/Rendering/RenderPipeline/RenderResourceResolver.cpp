#include "enginepch.h"
#include "RenderResourceResolver.h"

#include "HuaEngine/Rendering/Mesh/MeshManager.h"
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

		Ref<VertexArray> ResolveCachedVertexArray(const RenderItem& item) {
			if (!item.SourceEntity.IsValid()) {
				return nullptr;
			}

			const auto* meshComponent = item.SourceEntity.TryGetComponent<MeshComponent>();
			return meshComponent != nullptr ? meshComponent->m_CachedVertexArray : nullptr;
		}
	}

	bool RenderResourceResolver::Resolve(
		const RenderItem& item,
		ResolvedRenderItem& outResolvedItem,
		std::vector<RenderDiagnostic>& diagnostics) const {
		outResolvedItem = {};
		outResolvedItem.Source = &item;

		Ref<Mesh> mesh = nullptr;
		Ref<VertexArray> vertexArray = nullptr;
		if (!item.MeshAssetName.empty()) {
			mesh = MeshManager::Instance().GetMesh(item.MeshAssetName);
			if (mesh) {
				vertexArray = mesh->GetVertexArray();
			}
		}

		if (!vertexArray) {
			vertexArray = ResolveCachedVertexArray(item);
		}

		if (!vertexArray && !mesh) {
			AddDiagnostic(
				diagnostics,
				RenderDiagnosticCode::MissingMeshAsset,
				item.SourceEntity,
				"Render item mesh asset is missing: " + item.MeshAssetName);
			return false;
		}

		if (!vertexArray) {
			AddDiagnostic(
				diagnostics,
				RenderDiagnosticCode::MissingVertexArray,
				item.SourceEntity,
				"Render item mesh does not provide a vertex array: " + item.MeshAssetName);
			return false;
		}

		if (!item.MaterialInstanceRef) {
			AddDiagnostic(
				diagnostics,
				RenderDiagnosticCode::MissingMaterialInstance,
				item.SourceEntity,
				"Render item material instance is missing");
			return false;
		}

		auto baseMaterial = item.MaterialInstanceRef->GetBaseMaterial();
		if (!baseMaterial) {
			AddDiagnostic(
				diagnostics,
				RenderDiagnosticCode::MissingBaseMaterial,
				item.SourceEntity,
				"Render item material instance has no base material");
			return false;
		}

		if (!baseMaterial->GetShader()) {
			AddDiagnostic(
				diagnostics,
				RenderDiagnosticCode::MissingShader,
				item.SourceEntity,
				"Render item base material has no shader");
			return false;
		}

		outResolvedItem.VertexArrayRef = vertexArray;
		outResolvedItem.MaterialInstanceRef = item.MaterialInstanceRef;
		return true;
	}
}
