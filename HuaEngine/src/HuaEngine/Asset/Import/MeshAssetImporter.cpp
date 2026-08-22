#include "enginepch.h"
#include "MeshAssetImporter.h"

namespace HE {
	bool MeshAssetImporter::CanImport(AssetKind kind, std::string_view extension) const {
		return kind == AssetKind::Mesh && extension == ".mesh";
	}

	AssetImportResult MeshAssetImporter::Import(const AssetImportContext& context) const {
		AssetImportResult result;
		auto mesh = Rendering::Mesh::LoadFromFile(context.SourcePath.generic_string());
		if (!mesh) {
			result.Diagnostics.push_back({
				DiagnosticSeverity::Error,
				"asset.import.mesh_deserialize_failed",
				"Mesh source file could not be deserialized",
				context.SourcePath.generic_string()
			});
			return result;
		}

		auto encodeResult = EncodeMeshArtifact(*mesh, result.Artifact);
		if (!encodeResult.Succeeded()) {
			result.Diagnostics = std::move(encodeResult.Details);
			return result;
		}

		result.Success = true;
		return result;
	}
}
