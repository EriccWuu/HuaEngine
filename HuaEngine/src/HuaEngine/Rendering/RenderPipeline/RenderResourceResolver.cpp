#include "enginepch.h"
#include "RenderResourceResolver.h"

#include "HuaEngine/Asset/AssetTypes.h"
#include "HuaEngine/Rendering/Material/MaterialLibrary.h"
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

		std::string MeshNameFromGuid(const AssetGuid& guid) {
			if (guid == BuiltinAssetGuids::QuadMesh || guid == BuiltinAssetGuids::FallbackMesh) {
				return "Quad";
			}
			if (guid == BuiltinAssetGuids::CubeMesh) {
				return "Cube";
			}
			if (guid == BuiltinAssetGuids::SphereMesh) {
				return "Sphere";
			}
			return {};
		}

		Ref<Material> ResolveBaseMaterial(const AssetGuid& guid) {
			if (guid.empty()) {
				return nullptr;
			}

			auto& library = MaterialLibrary::Instance();
			if (!library.GetDefaultMaterial()) {
				library.CreateDefaultMaterials();
			}

			if (guid == BuiltinAssetGuids::DefaultMaterial || guid == BuiltinAssetGuids::FallbackMaterial) {
				return library.GetDefaultMaterial();
			}

			return nullptr;
		}
	}

	bool RenderResourceResolver::Resolve(
		const RenderItem& item,
		ResolvedRenderItem& outResolvedItem,
		std::vector<RenderDiagnostic>& diagnostics) const {
		outResolvedItem = {};
		outResolvedItem.Source = &item;

		Ref<VertexArray> vertexArray = nullptr;
		const std::string meshName = MeshNameFromGuid(item.Mesh.Reference.Guid);
		if (!meshName.empty()) {
			MeshManager::Instance().LoadDefaultMeshes();
			Ref<Mesh> mesh = MeshManager::Instance().GetMesh(meshName);
			if (mesh) {
				vertexArray = mesh->GetVertexArray();
			}
		}

		if (!vertexArray) {
			AddDiagnostic(
				diagnostics,
				RenderDiagnosticCode::MissingMeshAsset,
				item.SourceEntity,
				"Render item mesh asset is missing: " + item.Mesh.Reference.Guid);
			return false;
		}

		Ref<Material> baseMaterial = ResolveBaseMaterial(item.Material.Reference.Guid);
		if (!baseMaterial) {
			AddDiagnostic(
				diagnostics,
				RenderDiagnosticCode::MissingBaseMaterial,
				item.SourceEntity,
				"Render item material asset is missing: " + item.Material.Reference.Guid);
			return false;
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

		if (!baseMaterial->GetShader()) {
			AddDiagnostic(
				diagnostics,
				RenderDiagnosticCode::MissingShader,
				item.SourceEntity,
				"Render item base material has no shader");
			return false;
		}

		for (const auto& [parameterName, value] : item.MaterialOverrides.Parameters) {
			materialInstance->SetParameter(parameterName, value);
		}

		outResolvedItem.VertexArrayRef = vertexArray;
		outResolvedItem.MaterialInstanceRef = materialInstance;
		return true;
	}
}
