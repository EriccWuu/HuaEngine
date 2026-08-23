#include "enginepch.h"
#include "MaterialAssetImporter.h"

#include <algorithm>

#include "HuaEngine/Asset/AssetSourcePath.h"

namespace HE {
	bool MaterialAssetImporter::CanImport(AssetKind kind, std::string_view extension) const {
		return kind == AssetKind::Material && (extension == ".material" || extension == ".mat");
	}

	AssetImportResult MaterialAssetImporter::Import(const AssetImportContext& context) const {
		AssetImportResult result;
		std::vector<AssetGuid> dependencies;
		Rendering::MaterialSourceData sourceData;
		auto loadResult = Rendering::LoadMaterialSourceData(context.SourcePath, sourceData);
		if (!loadResult.Succeeded()) {
			result.Diagnostics = std::move(loadResult.Details);
			return result;
		}
		if (!sourceData.ShaderPath.empty()) {
			auto shaderRecord = context.SourceAsset;
			shaderRecord.RelativePath = sourceData.ShaderPath;
			std::filesystem::path shaderSourcePath;
			auto shaderPathResult = ResolveAssetSourcePath(context.Project, shaderRecord, shaderSourcePath);
			std::error_code errorCode;
			if (!shaderPathResult.Succeeded() || !std::filesystem::is_regular_file(shaderSourcePath, errorCode)) {
				result.Diagnostics = std::move(shaderPathResult.Details);
				result.Diagnostics.push_back({
					DiagnosticSeverity::Error,
					"asset.import.material_shader_missing",
					"Material shader path must reference a file under the material source root",
					sourceData.ShaderPath
				});
				return result;
			}
		}

		for (auto& [name, parameter] : sourceData.Parameters) {
			(void)name;
			if (parameter.Type != Rendering::MaterialParameterType::Texture2D) continue;
			auto& textureReference = std::get<std::string>(parameter.Value);
			if (textureReference.empty()) continue;

			const auto* textureRecord = context.Manifest ? context.Manifest->FindByAssetId(textureReference) : nullptr;
			if (!textureRecord || textureRecord->Kind != AssetKind::Texture2D) {
				result.Diagnostics.push_back({
					DiagnosticSeverity::Warning,
					"asset.import.material_texture_unresolved",
					"Material texture reference is not registered in the asset manifest and was cleared",
					textureReference
				});
				textureReference.clear();
				continue;
			}

			textureReference = textureRecord->Guid;
			dependencies.push_back(textureRecord->Guid);
		}

		auto encodeResult = EncodeMaterialArtifact(sourceData, result.Artifact);
		if (!encodeResult.Succeeded()) {
			result.Diagnostics.insert(result.Diagnostics.end(), encodeResult.Details.begin(), encodeResult.Details.end());
			return result;
		}
		std::sort(dependencies.begin(), dependencies.end());
		result.Artifact.Dependencies = std::move(dependencies);
		result.Artifact.Dependencies.erase(
			std::unique(result.Artifact.Dependencies.begin(), result.Artifact.Dependencies.end()),
			result.Artifact.Dependencies.end());
		result.Success = true;
		return result;
	}
}
