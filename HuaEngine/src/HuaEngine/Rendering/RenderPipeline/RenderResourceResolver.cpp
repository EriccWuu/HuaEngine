#include "enginepch.h"
#include "RenderResourceResolver.h"

#include "HuaEngine/Rendering/Mesh/MeshManager.h"

namespace HE::Rendering {
	namespace {
		void AddDiagnostic(
			std::vector<RenderDiagnostic>& diagnostics,
			RenderDiagnosticCode code,
			Entity sourceEntity,
			std::string message) {
			diagnostics.push_back({ code, sourceEntity, std::move(message) });
		}
	}

	bool RenderResourceResolver::Resolve(
		const RenderItem& item,
		ResolvedRenderItem& outResolvedItem,
		std::vector<RenderDiagnostic>& diagnostics) const {
		outResolvedItem = {};
		outResolvedItem.Source = &item;

		auto mesh = MeshManager::Instance().GetMesh(item.MeshAssetName);
		if (!mesh) {
			AddDiagnostic(
				diagnostics,
				RenderDiagnosticCode::MissingMeshAsset,
				item.SourceEntity,
				"Render item mesh asset is missing: " + item.MeshAssetName);
			return false;
		}

		auto vertexArray = mesh->GetVertexArray();
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
