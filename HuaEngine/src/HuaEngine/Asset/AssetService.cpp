#include "enginepch.h"
#include "AssetService.h"

#include <algorithm>
#include <system_error>
#include <unordered_set>
#include <vector>

#include "AssetResolver.h"
#include "AssetSourcePath.h"
#include "BuiltinAssetCatalog.h"
#include "HuaEngine/Asset/Metadata/AssetMeta.h"
#include "HuaEngine/Asset/Import/MeshAssetImporter.h"
#include "HuaEngine/Asset/Import/ObjMeshImporter.h"
#include "HuaEngine/Asset/Import/MaterialAssetImporter.h"
#include "HuaEngine/Asset/Import/PngTextureImporter.h"
#include "HuaEngine/Asset/Import/AssetSourceHash.h"
#include "HuaEngine/Asset/Import/HlslShaderImporter.h"
#include "HuaEngine/Asset/Artifact/MaterialArtifact.h"
#include "HuaEngine/Asset/Artifact/MeshArtifact.h"
#include "HuaEngine/Asset/Artifact/TextureArtifact.h"
#include "HuaEngine/Asset/Artifact/ShaderArtifact.h"
#include "HuaEngine/Rendering/Material/MaterialLibrary.h"
#include "HuaEngine/Rendering/Material/MaterialSerializer.h"
#include "HuaEngine/Rendering/Mesh/MeshManager.h"
#include "HuaEngine/Serialization/Serialization.h"
#include "stb_image.h"

namespace {
	struct NormalizedAssetPath {
		std::string AssetId;
		std::filesystem::path RelativePath;
		std::filesystem::path AbsolutePath;
		bool ExistsOnDisk = false;
	};

	struct ReimportCandidate {
		std::string AssetId;
		std::filesystem::path RelativePath;
		HE::AssetKind Kind = HE::AssetKind::Unknown;
	};

	std::string HandleToString(HE::AssetHandle handle) {
		return std::to_string(handle);
	}

	std::string CountToString(uint32_t value) {
		return std::to_string(value);
	}

	bool IsEscapingAssetRoot(const std::filesystem::path& relativePath) {
		const auto normalized = relativePath.generic_string();
		return normalized == ".." || normalized.rfind("../", 0) == 0 || normalized.find("/../") != std::string::npos;
	}

	bool IsOutsideAssetRoot(const std::filesystem::path& assetRoot, const std::filesystem::path& absolutePath) {
		if (assetRoot.empty() || absolutePath.empty()) {
			return true;
		}

		const auto relativePath = absolutePath.lexically_relative(assetRoot);
		if (relativePath.empty() && absolutePath != assetRoot) {
			return true;
		}

		return relativePath.is_absolute() || IsEscapingAssetRoot(relativePath);
	}

	bool TryResolveReimportTarget(
		const HE::ProjectContext& context,
		const std::filesystem::path& targetPath,
		std::filesystem::path& outAssetRoot,
		std::filesystem::path& outTargetPath,
		HE::ResultEnvelope& outError) {
		if (!context.IsLoaded()) {
			outError = HE::ResultEnvelope::Failure("asset.reimport", targetPath.generic_string(), "Project context is not loaded");
			outError.AddDetail({ HE::DiagnosticSeverity::Error, "asset.project.unloaded", "Asset operations require a loaded project context", {} });
			return false;
		}
		if (targetPath.empty()) {
			outError = HE::ResultEnvelope::Failure("asset.reimport", {}, "Reimport target path is empty");
			outError.AddDetail({ HE::DiagnosticSeverity::Error, "asset.reimport.target_empty", "Reimport requires a file or directory under the project asset root", {} });
			return false;
		}

		std::error_code errorCode;
		outAssetRoot = std::filesystem::weakly_canonical(context.GetAssetRootPath(), errorCode);
		if (errorCode) {
			outError = HE::ResultEnvelope::Failure("asset.reimport", context.GetAssetRootPath().generic_string(), "Failed to resolve project asset root");
			outError.AddDetail({ HE::DiagnosticSeverity::Error, "asset.root.resolve_failed", errorCode.message(), context.GetAssetRootPath().generic_string() });
			return false;
		}

		const auto candidatePath = targetPath.is_absolute() ? targetPath : outAssetRoot / targetPath;
		outTargetPath = std::filesystem::weakly_canonical(candidatePath, errorCode);
		if (errorCode || IsOutsideAssetRoot(outAssetRoot, outTargetPath)) {
			outError = HE::ResultEnvelope::Failure("asset.reimport", targetPath.generic_string(), "Reimport target is outside the project asset root");
			outError.AddDetail({ HE::DiagnosticSeverity::Error, "asset.path.outside_root", errorCode ? errorCode.message() : "Path escapes the asset root", targetPath.generic_string() });
			return false;
		}

		if (!std::filesystem::exists(outTargetPath, errorCode) || errorCode) {
			outError = HE::ResultEnvelope::Failure("asset.reimport", outTargetPath.generic_string(), "Reimport target does not exist");
			outError.AddDetail({ HE::DiagnosticSeverity::Error, "asset.reimport.target_missing", errorCode ? errorCode.message() : "Target was not found", outTargetPath.generic_string() });
			return false;
		}
		return true;
	}

	bool TryNormalizeAssetPath(
		const HE::ProjectContext& context,
		std::string_view assetId,
		NormalizedAssetPath& outPath,
		HE::ResultEnvelope& outError) {
		if (!context.IsLoaded()) {
			outError = HE::ResultEnvelope::Failure("asset.path.normalize", std::string(assetId), "Project context is not loaded");
			outError.AddDetail({ HE::DiagnosticSeverity::Error, "asset.project.unloaded", "Asset operations require a loaded project context", {} });
			return false;
		}

		if (assetId.empty()) {
			outError = HE::ResultEnvelope::Failure("asset.path.normalize", {}, "Asset id is empty");
			outError.AddDetail({ HE::DiagnosticSeverity::Error, "asset.id.empty", "Asset id must be a relative path under the project asset root", {} });
			return false;
		}

		std::error_code errorCode;
		const auto assetRoot = std::filesystem::absolute(context.GetAssetRootPath(), errorCode).lexically_normal();
		if (errorCode) {
			outError = HE::ResultEnvelope::Failure("asset.path.normalize", std::string(assetId), "Failed to resolve project asset root");
			outError.AddDetail({ HE::DiagnosticSeverity::Error, "asset.root.resolve_failed", errorCode.message(), context.GetAssetRootPath().generic_string() });
			return false;
		}

		std::filesystem::path inputPath(assetId);
		std::filesystem::path relativePath;
		if (inputPath.is_absolute()) {
			relativePath = std::filesystem::relative(inputPath, assetRoot, errorCode);
			if (errorCode) {
				outError = HE::ResultEnvelope::Failure("asset.path.normalize", inputPath.generic_string(), "Asset path is outside the project asset root");
				outError.AddDetail({ HE::DiagnosticSeverity::Error, "asset.path.outside_root", errorCode.message(), inputPath.generic_string() });
				return false;
			}
		}
		else {
			relativePath = inputPath;
		}

		relativePath = relativePath.lexically_normal();
		if (relativePath.empty() || relativePath == ".") {
			outError = HE::ResultEnvelope::Failure("asset.path.normalize", std::string(assetId), "Asset id normalized to an empty path");
			outError.AddDetail({ HE::DiagnosticSeverity::Error, "asset.id.invalid", "Asset id must not normalize to the asset root itself", std::string(assetId) });
			return false;
		}

		if (relativePath.is_absolute() || IsEscapingAssetRoot(relativePath)) {
			outError = HE::ResultEnvelope::Failure("asset.path.normalize", std::string(assetId), "Asset id escapes the project asset root");
			outError.AddDetail({ HE::DiagnosticSeverity::Error, "asset.id.escapes_root", "Asset id must remain within the asset root", relativePath.generic_string() });
			return false;
		}

		outPath.RelativePath = relativePath;
		outPath.AssetId = relativePath.generic_string();
		outPath.AbsolutePath = (assetRoot / relativePath).lexically_normal();
		outPath.ExistsOnDisk = std::filesystem::exists(outPath.AbsolutePath, errorCode);
		return true;
	}

	HE::ResultEnvelope MakeRegistrationResult(
		std::string operation,
		const HE::AssetRecord& record,
		std::string summary) {
		auto result = HE::ResultEnvelope::Success(std::move(operation), record.AssetId, std::move(summary));
		result.SetPayloadValue("asset_handle", HandleToString(record.Handle));
		result.SetPayloadValue("asset_guid", record.Guid);
		result.SetPayloadValue("asset_id", record.AssetId);
		result.SetPayloadValue("asset_kind", std::string(HE::ToString(record.Kind)));
		result.SetPayloadValue("asset_source", std::string(HE::ToString(record.Source)));
		result.SetPayloadValue("import_state", std::string(HE::ToString(record.ImportState)));
		result.SetPayloadValue("asset_path", record.AbsolutePath.generic_string());
		result.SetPayloadValue("exists_on_disk", record.ExistsOnDisk ? "true" : "false");
		return result;
	}

	HE::AssetGuid GetExistingGuidOrGenerate(const HE::AssetRegistry& registry, const HE::AssetManifest& manifest, std::string_view assetId) {
		if (const auto* existing = manifest.FindByAssetId(assetId)) {
			if (!existing->Guid.empty()) {
				return existing->Guid;
			}
		}
		if (const auto* existing = registry.Find(assetId)) {
			if (!existing->Guid.empty()) {
				return existing->Guid;
			}
		}

		return HE::GenerateAssetGuid();
	}

	HE::ResultEnvelope MakeResolveFailure(std::string operation, std::string target, std::string summary) {
		auto result = HE::ResultEnvelope::Failure(std::move(operation), std::move(target), std::move(summary));
		result.AddDetail({ HE::DiagnosticSeverity::Error, "asset.lookup.missing", "Requested asset could not be found in the registry", {} });
		return result;
	}

	HE::AssetRecord MakeRegistryRecord(
		const HE::ProjectContext& context,
		const HE::AssetManifestRecord& manifestRecord,
		HE::AssetHandle handle = 0) {
		HE::AssetRecord record;
		record.Handle = handle;
		record.Guid = manifestRecord.Guid;
		record.Kind = manifestRecord.Kind;
		record.Source = manifestRecord.Source;
		record.AssetId = manifestRecord.AssetId;
		record.RelativePath = manifestRecord.RelativePath;
		record.BuiltinName = manifestRecord.BuiltinName;
		record.ImportState = manifestRecord.ImportState;
		if (record.Source == HE::AssetSource::File || record.Source == HE::AssetSource::Builtin) {
			HE::AssetManifestRecord sourceRecord = manifestRecord;
			(void)HE::ResolveAssetSourcePath(context, sourceRecord, record.AbsolutePath);
			std::error_code errorCode;
			record.ExistsOnDisk = std::filesystem::is_regular_file(record.AbsolutePath, errorCode);
		}
		return record;
	}

	HE::Ref<HE::Rendering::Mesh> CreateBuiltinMesh(
		HE::BuiltinMeshPrimitive primitive,
		std::string_view meshName) {
		const std::string resolvedMeshName = meshName.empty()
			? std::string(HE::ToString(primitive))
			: std::string(meshName);

		switch (primitive) {
		case HE::BuiltinMeshPrimitive::Quad:
			return HE::Rendering::Mesh::CreateQuad(resolvedMeshName);
		case HE::BuiltinMeshPrimitive::Cube:
			return HE::Rendering::Mesh::CreateCube(resolvedMeshName);
		case HE::BuiltinMeshPrimitive::Sphere:
			return HE::Rendering::Mesh::CreateSphere(resolvedMeshName);
		}

		return nullptr;
	}
}

namespace HE {
	ResultEnvelope AssetService::GetAssetImportHealth(const AssetGuid& guid, AssetImportHealth& outHealth) const {
		outHealth = { .State = AssetImportHealthState::Missing, .Guid = guid };
		const auto* record = m_Manifest.FindByGuid(guid);
		if (!record || !m_ProjectContext) {
			return ResultEnvelope::Failure("asset.import_health", guid, "Asset metadata is unavailable");
		}
		if (record->Kind == AssetKind::Scene) {
			std::error_code errorCode;
			const bool available = std::filesystem::is_regular_file(m_ProjectContext->GetAssetRootPath() / record->RelativePath, errorCode);
			outHealth.State = available ? AssetImportHealthState::Current : AssetImportHealthState::Missing;
			return ResultEnvelope::Success("asset.import_health", guid, available ? "Native scene source is available" : "Native scene source is missing");
		}
		std::filesystem::path sourcePath;
		auto sourceResult = ResolveAssetSourcePath(*m_ProjectContext, *record, sourcePath);
		const auto* importer = sourceResult.Succeeded() ? m_ImporterRegistry.Find(record->Kind, sourcePath.extension().string()) : nullptr;
		const auto* libraryRecord = m_Library.Find(guid);
		const bool artifactAvailable = importer && m_Library.IsArtifactAvailable(
			guid,
			record->Kind,
			importer->GetId(),
			importer->GetVersion(),
			importer->GetArtifactVersion());
		if (const auto failure = m_LastImportFailures.find(guid); failure != m_LastImportFailures.end()) {
			outHealth.State = artifactAvailable ? AssetImportHealthState::LastGoodWithFailure : AssetImportHealthState::Missing;
			outHealth.Diagnostics = failure->second;
			return ResultEnvelope::Success("asset.import_health", guid, artifactAvailable ? "Last-good artifact is active after an import failure" : "Import failed and no compatible artifact is available");
		}
		if (!libraryRecord) {
			return ResultEnvelope::Success("asset.import_health", guid, "Asset artifact is missing");
		}
		std::error_code errorCode;
		if (!artifactAvailable || !sourceResult.Succeeded() || !std::filesystem::is_regular_file(sourcePath, errorCode)) {
			outHealth.State = AssetImportHealthState::Stale;
			return ResultEnvelope::Success("asset.import_health", guid, "Asset artifact is stale");
		}
		outHealth.State = AssetImportHealthState::Current;
		return ResultEnvelope::Success("asset.import_health", guid, "Asset artifact is current");
	}

	ResultEnvelope AssetService::InspectAsset(const AssetGuid& guid, AssetInspectionSnapshot& outSnapshot) const {
		outSnapshot = {};
		const auto* asset = FindRecordByGuid(guid);
		const auto* manifestRecord = m_Manifest.FindByGuid(guid);
		if (!asset || !manifestRecord) {
			return ResultEnvelope::Failure("asset.inspect", guid, "Asset metadata is unavailable");
		}

		outSnapshot.Asset = *asset;
		(void)GetAssetImportHealth(guid, outSnapshot.Health);
		outSnapshot.Diagnostics = outSnapshot.Health.Diagnostics;
		if (asset->Kind == AssetKind::Scene) {
			outSnapshot.ImporterId = "scene.native";
			outSnapshot.ImporterVersion = 1;
		}
		if (asset->Source == AssetSource::File && m_ProjectContext) {
			(void)ComputeAssetSourceHash(asset->AbsolutePath, outSnapshot.SourceContentHash);
			(void)ComputeAssetSourceHash(GetAssetMetaPath(asset->AbsolutePath), outSnapshot.MetaContentHash);
			AssetMeta meta;
			if (LoadAssetMeta(asset->AbsolutePath, meta).Succeeded()) outSnapshot.SettingsVersion = meta.SettingsVersion;
		}

		if (const auto* libraryRecord = m_Library.Find(guid)) {
			outSnapshot.ImporterId = libraryRecord->ImporterId;
			outSnapshot.ImporterVersion = libraryRecord->ImporterVersion;
			outSnapshot.ImportFingerprint = libraryRecord->ImportFingerprint;
			outSnapshot.ArtifactRelativePath = libraryRecord->ArtifactRelativePath;
			outSnapshot.Dependencies = libraryRecord->Dependencies;
			outSnapshot.Dependents = m_Library.FindDependents(guid);
			AssetArtifact artifact;
			if (m_Library.ReadArtifact(guid, artifact).Succeeded() && asset->Kind == AssetKind::Mesh) {
				Ref<Rendering::Mesh> mesh;
				if (DecodeMeshArtifact(artifact, mesh).Succeeded() && mesh) {
					const auto& data = mesh->GetMeshData();
					MeshArtifactStatistics statistics;
					statistics.VertexCount = data.Layout.Stride == 0 ? 0 : static_cast<uint32_t>(data.VertexData.size() * sizeof(float) / data.Layout.Stride);
					statistics.IndexCount = static_cast<uint32_t>(data.IndexData.size());
					for (const auto& element : data.Layout.Elements) {
						statistics.HasUv |= element.Name == "a_TexCoord";
						statistics.HasNormals |= element.Name == "a_Normal";
						statistics.HasTangents |= element.Name == "a_Tangent";
					}
					if (statistics.VertexCount > 0 && data.Layout.Stride >= 12) {
						statistics.BoundsMin = statistics.BoundsMax = { data.VertexData[0], data.VertexData[1], data.VertexData[2] };
						const size_t stride = data.Layout.Stride / sizeof(float);
						for (size_t index = 0; index < data.VertexData.size(); index += stride) for (size_t axis = 0; axis < 3; ++axis) {
							statistics.BoundsMin[axis] = (std::min)(statistics.BoundsMin[axis], data.VertexData[index + axis]);
							statistics.BoundsMax[axis] = (std::max)(statistics.BoundsMax[axis], data.VertexData[index + axis]);
						}
					}
					outSnapshot.MeshStatistics = statistics;
				}
			}
			if (m_Library.ReadArtifact(guid, artifact).Succeeded() && asset->Kind == AssetKind::Texture2D) {
				TextureArtifactData texture;
				if (DecodeTextureArtifact(artifact, texture).Succeeded()) {
					TextureArtifactStatistics statistics{ .Width = texture.Width, .Height = texture.Height, .MipLevels = texture.MipLevels };
					int width = 0, height = 0, channels = 0;
					if (stbi_info(asset->AbsolutePath.string().c_str(), &width, &height, &channels)) { statistics.SourceWidth = width; statistics.SourceHeight = height; statistics.SourceChannels = channels; statistics.HasAlpha = channels == 2 || channels == 4; }
					outSnapshot.TextureStatistics = statistics;
				}
			}
			if (m_Library.ReadArtifact(guid, artifact).Succeeded() && asset->Kind == AssetKind::Shader) {
				ShaderArtifactDataV2 shader;
				if (DecodeShaderArtifactV2(artifact, shader).Succeeded()) outSnapshot.ShaderData = std::move(shader);
			}
		}
		else if (m_ProjectContext) {
			std::filesystem::path sourcePath;
			if (ResolveAssetSourcePath(*m_ProjectContext, *manifestRecord, sourcePath).Succeeded()) {
				if (const auto* importer = m_ImporterRegistry.Find(manifestRecord->Kind, sourcePath.extension().string())) {
					outSnapshot.ImporterId = std::string(importer->GetId());
					outSnapshot.ImporterVersion = importer->GetVersion();
				}
			}
		}

		auto result = ResultEnvelope::Success("asset.inspect", guid, "Asset inspection snapshot created");
		result.SetPayloadValue("dependency_count", std::to_string(outSnapshot.Dependencies.size()));
		result.SetPayloadValue("dependent_count", std::to_string(outSnapshot.Dependents.size()));
		return result;
	}

	ResultEnvelope AssetService::GetMaterialDefinition(const AssetGuid& materialGuid, Rendering::MaterialDefinition& outDefinition, AssetImportHealth* outHealth) const {
		outDefinition = {};
		AssetImportHealth materialHealth;
		AssetImportHealth shaderHealth;
		auto publishHealth = [&](AssetImportHealth health) {
			if (outHealth) *outHealth = std::move(health);
		};
		const auto* materialRecord = m_Manifest.FindByGuid(materialGuid);
		if (!materialRecord || materialRecord->Kind != AssetKind::Material) {
			publishHealth({ .State = AssetImportHealthState::Missing, .Guid = materialGuid });
			return ResultEnvelope::Failure("asset.material_definition", materialGuid, "Material asset was not found");
		}
		(void)GetAssetImportHealth(materialGuid, materialHealth);
		if (materialHealth.State == AssetImportHealthState::Missing || materialHealth.State == AssetImportHealthState::Stale) {
			publishHealth(materialHealth);
			return ResultEnvelope::ManualIntervention("asset.material_definition", materialGuid, materialHealth.State == AssetImportHealthState::Missing ? "Material artifact is unavailable" : "Material artifact requires reimport");
		}
		AssetArtifact materialArtifact;
		Rendering::MaterialSourceData material;
		if (!m_Library.ReadArtifact(materialGuid, materialArtifact).Succeeded() || !DecodeMaterialArtifact(materialArtifact, material).Succeeded()) {
			materialHealth.State = AssetImportHealthState::Missing;
			publishHealth(materialHealth);
			return ResultEnvelope::ManualIntervention("asset.material_definition", materialGuid, "Material artifact is unavailable");
		}
		(void)GetAssetImportHealth(material.ShaderGuid, shaderHealth);
		if (shaderHealth.State == AssetImportHealthState::Missing || shaderHealth.State == AssetImportHealthState::Stale) {
			publishHealth(shaderHealth);
			return ResultEnvelope::ManualIntervention("asset.material_definition", materialGuid, shaderHealth.State == AssetImportHealthState::Missing ? "Shader artifact is unavailable" : "Shader artifact requires reimport");
		}
		AssetArtifact shaderArtifact;
		ShaderArtifactDataV2 shader;
		if (!m_Library.ReadArtifact(material.ShaderGuid, shaderArtifact).Succeeded() || !DecodeShaderArtifactV2(shaderArtifact, shader).Succeeded()) {
			shaderHealth.State = AssetImportHealthState::Missing;
			publishHealth(shaderHealth);
			return ResultEnvelope::ManualIntervention("asset.material_definition", materialGuid, "Shader artifact is unavailable");
		}
		if (material.ShaderInterfaceDigest != shader.Interface.Gpu.Digest) {
			materialHealth.State = AssetImportHealthState::Stale;
			publishHealth(materialHealth);
			return ResultEnvelope::ManualIntervention("asset.material_definition", materialGuid, "Material artifact requires reimport");
		}
		std::vector<Rendering::MaterialParameterDefinition> parameters;
		for (const auto& metadata : shader.Interface.Authoring.Parameters) {
			if (metadata.Scope != Rendering::ShaderParameterScope::Material) continue;
			Rendering::MaterialParameterDefinition definition{
				.Name = metadata.Name, .DisplayName = metadata.DisplayName, .Type = metadata.Type, .Editor = metadata.Editor,
				.DefaultValue = metadata.DefaultValue, .CurrentValue = metadata.DefaultValue, .Range = metadata.Range, .Step = metadata.Step, .Tooltip = metadata.Tooltip
			};
			if (const auto current = material.Parameters.find(metadata.Name); current != material.Parameters.end()) {
				std::visit([&](const auto& value) {
					using T = std::decay_t<decltype(value)>;
					if constexpr (std::is_same_v<T, int> || std::is_same_v<T, float> || std::is_same_v<T, glm::vec2> || std::is_same_v<T, glm::vec3> || std::is_same_v<T, glm::vec4> || std::is_same_v<T, glm::mat4> || std::is_same_v<T, std::string>) definition.CurrentValue = value;
				}, current->second.Value);
			}
			parameters.push_back(std::move(definition));
		}
		outDefinition = Rendering::MaterialDefinition(std::move(parameters), material.MaterialDefinitionDigest);
		outDefinition.SetIdentity(materialGuid, material.ShaderGuid, material.ShaderInterfaceDigest, material.ShaderInterfaceSignature);
		AssetImportHealth combinedHealth = materialHealth;
		if (shaderHealth.State == AssetImportHealthState::LastGoodWithFailure) {
			combinedHealth.State = AssetImportHealthState::LastGoodWithFailure;
			combinedHealth.Diagnostics.insert(combinedHealth.Diagnostics.end(), shaderHealth.Diagnostics.begin(), shaderHealth.Diagnostics.end());
		}
		publishHealth(combinedHealth);
		auto result = ResultEnvelope::Success("asset.material_definition", materialGuid, combinedHealth.State == AssetImportHealthState::LastGoodWithFailure ? "Material definition resolved from last-good artifacts" : "Material definition resolved");
		if (combinedHealth.State == AssetImportHealthState::LastGoodWithFailure) {
			for (const auto& diagnostic : combinedHealth.Diagnostics) result.AddDetail(diagnostic);
		}
		return result;
	}

	AssetService::AssetService() {
		const bool meshRegistered = m_ImporterRegistry.Register(std::make_unique<MeshAssetImporter>());
		const bool objMeshRegistered = m_ImporterRegistry.Register(std::make_unique<ObjMeshImporter>());
		const bool materialRegistered = m_ImporterRegistry.Register(std::make_unique<MaterialAssetImporter>());
		const bool textureRegistered = m_ImporterRegistry.Register(std::make_unique<PngTextureImporter>());
		const bool hlslShaderRegistered = m_ImporterRegistry.Register(std::make_unique<HlslShaderImporter>());
		HE_CORE_ASSERT(meshRegistered && objMeshRegistered && materialRegistered && textureRegistered && hlslShaderRegistered, "Failed to register core asset importers");
	}

	ResultEnvelope AssetService::LoadOrCreateManifest(const ProjectContext& context) {
		return LoadOrCreateManifestInternal(context, true);
	}

	ResultEnvelope AssetService::LoadOrCreateManifestInternal(
		const ProjectContext& context,
		bool resetRuntimeCache) {
		AssetManifest loadedManifest;
		auto result = LoadOrCreateAssetManifest(context, loadedManifest);
		if (!result.Succeeded()) {
			return result;
		}

		AssetManifest derivedManifest;
		std::unordered_map<std::string, size_t> legacyOrder;
		size_t legacyIndex = 0;
		loadedManifest.ForEachRecord([&](const AssetManifestRecord& record) {
			if (record.Source == AssetSource::Builtin) (void)derivedManifest.Upsert(record);
			else legacyOrder.emplace(record.AssetId, legacyIndex++);
		});
		std::vector<std::filesystem::path> sourcePaths;
		std::error_code scanError;
		std::filesystem::recursive_directory_iterator iterator(context.GetAssetRootPath(), std::filesystem::directory_options::skip_permission_denied, scanError);
		const std::filesystem::recursive_directory_iterator end;
		while (!scanError && iterator != end) {
			if (iterator->is_regular_file(scanError) && !scanError) {
				const auto& path = iterator->path();
				if (path.extension() == ".scene" || m_ImporterRegistry.FindByExtension(path.extension().string())) sourcePaths.push_back(path);
			}
			if (!scanError) iterator.increment(scanError);
		}
		if (scanError) return ResultEnvelope::Failure("asset.meta.migrate", context.GetAssetRootPath().generic_string(), "Failed to scan project asset sources");
		std::sort(sourcePaths.begin(), sourcePaths.end(), [&](const auto& left, const auto& right) {
			const auto leftId = left.lexically_relative(context.GetAssetRootPath()).lexically_normal().generic_string();
			const auto rightId = right.lexically_relative(context.GetAssetRootPath()).lexically_normal().generic_string();
			const auto leftOrder = legacyOrder.find(leftId);
			const auto rightOrder = legacyOrder.find(rightId);
			if (leftOrder != legacyOrder.end() || rightOrder != legacyOrder.end()) {
				if (leftOrder == legacyOrder.end()) return false;
				if (rightOrder == legacyOrder.end()) return true;
				return leftOrder->second < rightOrder->second;
			}
			return leftId < rightId;
		});
		std::unordered_set<AssetGuid> fileGuids;
		for (const auto& sourcePath : sourcePaths) {
			const auto relativePath = sourcePath.lexically_relative(context.GetAssetRootPath()).lexically_normal();
			const auto assetId = relativePath.generic_string();
			const auto importerMatch = m_ImporterRegistry.FindByExtension(sourcePath.extension().string());
			const AssetKind kind = sourcePath.extension() == ".scene" ? AssetKind::Scene : importerMatch->Kind;
			const std::string importerId = kind == AssetKind::Scene ? "scene.native" : std::string(importerMatch->Importer->GetId());
			const auto* legacyRecord = loadedManifest.FindByAssetId(assetId);
			AssetMeta meta;
			scanError.clear();
			const bool hasMeta = std::filesystem::is_regular_file(GetAssetMetaPath(sourcePath), scanError) && !scanError;
			if (hasMeta) {
				auto loadMetaResult = LoadAssetMeta(sourcePath, meta);
				if (!loadMetaResult.Succeeded()) return loadMetaResult;
				if (meta.ImporterId != importerId) return ResultEnvelope::Failure("asset.meta.migrate", assetId, "Asset metadata importer does not match the source type");
				if (legacyRecord && legacyRecord->Guid != meta.Guid) return ResultEnvelope::Failure("asset.meta.migrate", assetId, "Asset metadata GUID conflicts with the existing manifest");
				if (importerMatch) {
					std::unique_ptr<AssetImportSettings> settings;
					if (meta.SettingsVersion != importerMatch->Importer->GetSettingsVersion()) return ResultEnvelope::Failure("asset.meta.version_unsupported", assetId, "Asset settings version is unsupported");
					auto decodeResult = importerMatch->Importer->DecodeSettings(meta.Settings, settings);
					if (!decodeResult.Succeeded() || !settings) return decodeResult;
					auto validateResult = importerMatch->Importer->ValidateSettings(*settings);
					if (!validateResult.Succeeded()) return validateResult;
				}
			}
			else {
				meta.Guid = legacyRecord ? legacyRecord->Guid : GenerateAssetGuid();
				meta.ImporterId = importerId;
				meta.SettingsVersion = importerMatch ? importerMatch->Importer->GetSettingsVersion() : 1;
				if (auto saveMetaResult = SaveAssetMeta(sourcePath, meta); !saveMetaResult.Succeeded()) return saveMetaResult;
			}
			if (!fileGuids.insert(meta.Guid).second || derivedManifest.FindByGuid(meta.Guid)) return ResultEnvelope::Failure("asset.meta.migrate", assetId, "Asset metadata contains a duplicate GUID");
			AssetManifestRecord record{ .Guid = meta.Guid, .AssetId = assetId, .Kind = kind, .Source = AssetSource::File, .RelativePath = relativePath, .ImportState = AssetImportState::Registered };
			if (!derivedManifest.Upsert(std::move(record))) return ResultEnvelope::Failure("asset.meta.migrate", assetId, "Asset metadata conflicts with another source path");
		}
		if (auto saveManifestResult = SaveAssetManifest(context, derivedManifest); !saveManifestResult.Succeeded()) return saveManifestResult;
		loadedManifest = std::move(derivedManifest);

		m_Registry = AssetRegistry();
		if (resetRuntimeCache) {
			m_RuntimeCache = AssetRuntimeCache();
			m_LastImportFailures.clear();
		}
		m_Manifest = std::move(loadedManifest);
		m_ProjectContext = context;
		m_Manifest.ForEachRecord([&](const AssetManifestRecord& manifestRecord) {
			(void)m_Registry.Upsert(MakeRegistryRecord(context, manifestRecord));
		});
		return result;
	}

	ResultEnvelope AssetService::InitializeProjectAssets(
		const ProjectContext& context,
		AssetImportReport* outReport) {
		auto manifestResult = LoadOrCreateManifest(context);
		if (!manifestResult.Succeeded()) {
			manifestResult.Operation = "asset.initialize_project";
			return manifestResult;
		}

		auto libraryResult = m_Library.Open(context);
		if (!libraryResult.Succeeded()) {
			libraryResult.Operation = "asset.initialize_project";
			return libraryResult;
		}

		AssetImportService importService(m_ImporterRegistry, m_Library);
		AssetImportReport report;
		auto result = importService.ImportMissingAssets(context, m_Manifest, &report);
		for (const auto& guid : report.ImportedAssetGuids) m_LastImportFailures.erase(guid);
		for (const auto& failure : report.Failures) {
			auto diagnostics = failure.Diagnostics;
			if (diagnostics.empty()) diagnostics.push_back({ DiagnosticSeverity::Error, "asset.import.failed", "Asset import failed", failure.Guid });
			m_LastImportFailures[failure.Guid] = std::move(diagnostics);
		}
		if (outReport) *outReport = report;
		result.Operation = "asset.initialize_project";
		result.SetPayloadValue("library_path", m_Library.GetRootPath().generic_string());
		return result;
	}

	ResultEnvelope AssetService::GetShaderAuthoringMetadata(const AssetGuid& shaderGuid, Rendering::ShaderAuthoringMetadata& outMetadata) const {
		outMetadata = {};
		const auto* record = m_Manifest.FindByGuid(shaderGuid);
		if (!record || record->Kind != AssetKind::Shader) return ResultEnvelope::Failure("asset.shader_authoring_metadata", shaderGuid, "Shader asset was not found");
		AssetArtifact artifact;
		ShaderArtifactDataV2 shader;
		if (!m_Library.ReadArtifact(shaderGuid, artifact).Succeeded() || !DecodeShaderArtifactV2(artifact, shader).Succeeded()) return ResultEnvelope::ManualIntervention("asset.shader_authoring_metadata", shaderGuid, "Shader artifact is unavailable");
		outMetadata = std::move(shader.Interface.Authoring);
		return ResultEnvelope::Success("asset.shader_authoring_metadata", shaderGuid, "Shader authoring metadata loaded");
	}

	ResultEnvelope AssetService::RegisterSceneAsset(const ProjectContext& context, const std::filesystem::path& sourcePath, AssetGuid* outGuid) {
		NormalizedAssetPath normalizedPath;
		ResultEnvelope pathError;
		if (!TryNormalizeAssetPath(context, sourcePath.generic_string(), normalizedPath, pathError)) return pathError;
		if (normalizedPath.RelativePath.extension() != ".scene" || !normalizedPath.ExistsOnDisk) {
			return ResultEnvelope::Failure("asset.scene.register", normalizedPath.AssetId, "Scene source must be an existing .scene file under the asset root");
		}
		if (m_Manifest.Empty()) {
			auto loadResult = LoadOrCreateManifestInternal(context, false);
			if (!loadResult.Succeeded()) return loadResult;
		}
		const auto* existing = m_Manifest.FindByAssetId(normalizedPath.AssetId);
		if (existing && (existing->Kind != AssetKind::Scene || existing->Source != AssetSource::File)) {
			return ResultEnvelope::Failure("asset.scene.register", normalizedPath.AssetId, "Existing asset metadata conflicts with the scene source");
		}
		AssetMeta meta;
		std::error_code metaError;
		if (std::filesystem::is_regular_file(GetAssetMetaPath(normalizedPath.AbsolutePath), metaError) && !metaError) {
			auto loadMetaResult = LoadAssetMeta(normalizedPath.AbsolutePath, meta);
			if (!loadMetaResult.Succeeded()) return loadMetaResult;
			if (meta.ImporterId != "scene.native" || (existing && existing->Guid != meta.Guid)) return ResultEnvelope::Failure("asset.meta.guid_conflict", normalizedPath.AssetId, "Scene metadata conflicts with the existing identity");
		}
		else {
			meta.Guid = existing ? existing->Guid : GetExistingGuidOrGenerate(m_Registry, m_Manifest, normalizedPath.AssetId);
			meta.ImporterId = "scene.native";
			if (auto saveMetaResult = SaveAssetMeta(normalizedPath.AbsolutePath, meta); !saveMetaResult.Succeeded()) return saveMetaResult;
		}
		AssetManifestRecord manifestRecord{
			.Guid = meta.Guid,
			.AssetId = normalizedPath.AssetId,
			.Kind = AssetKind::Scene,
			.Source = AssetSource::File,
			.RelativePath = normalizedPath.RelativePath,
			.ImportState = AssetImportState::Registered
		};
		if (!m_Manifest.Upsert(manifestRecord)) return ResultEnvelope::Failure("asset.scene.register", normalizedPath.AssetId, "Asset manifest rejected the scene source");
		auto record = MakeRegistryRecord(context, manifestRecord);
		record.Handle = m_Registry.Upsert(record);
		if (record.Handle == 0) return ResultEnvelope::Failure("asset.scene.register", normalizedPath.AssetId, "Asset registry rejected the scene source");
		auto saveResult = SaveAssetManifest(context, m_Manifest);
		if (!saveResult.Succeeded()) return saveResult;
		if (outGuid) *outGuid = record.Guid;
		return MakeRegistrationResult("asset.scene.register", record, "Scene asset registered");
	}

	bool AssetService::CanImportSource(const std::filesystem::path& sourcePath) const {
		return m_ImporterRegistry.FindByExtension(sourcePath.extension().string()).has_value();
	}

	ResultEnvelope AssetService::ReimportAssets(
		const ProjectContext& context,
		const std::filesystem::path& targetPath,
		AssetReimportReport* outReport) {
		AssetReimportReport report;
		auto publishReport = [&report, outReport](ResultEnvelope result) {
			result.SetPayloadValue("scanned_files", CountToString(report.ScannedFiles));
			result.SetPayloadValue("supported_files", CountToString(report.SupportedFiles));
			result.SetPayloadValue("registered_assets", CountToString(report.RegisteredAssets));
			result.SetPayloadValue("reimported_assets", CountToString(report.ReimportedAssets));
			result.SetPayloadValue("skipped_files", CountToString(report.SkippedFiles));
			result.SetPayloadValue("failed_assets", CountToString(report.FailedAssets));
			if (outReport) {
				*outReport = report;
			}
			return result;
		};

		std::filesystem::path assetRoot;
		std::filesystem::path resolvedTarget;
		ResultEnvelope targetError;
		if (!TryResolveReimportTarget(context, targetPath, assetRoot, resolvedTarget, targetError)) {
			return publishReport(std::move(targetError));
		}

		std::vector<std::filesystem::path> sourcePaths;
		std::error_code errorCode;
		if (std::filesystem::is_regular_file(resolvedTarget, errorCode)) {
			sourcePaths.push_back(resolvedTarget);
		}
		else if (std::filesystem::is_directory(resolvedTarget, errorCode)) {
			std::filesystem::recursive_directory_iterator iterator(
				resolvedTarget,
				std::filesystem::directory_options::skip_permission_denied,
				errorCode);
			const std::filesystem::recursive_directory_iterator end;
			while (!errorCode && iterator != end) {
				if (iterator->is_regular_file(errorCode) && !errorCode && iterator->path().extension() != ".meta") {
					sourcePaths.push_back(std::filesystem::weakly_canonical(iterator->path(), errorCode));
				}
				if (!errorCode) {
					iterator.increment(errorCode);
				}
			}
		}
		else {
			errorCode = std::make_error_code(std::errc::invalid_argument);
		}
		if (errorCode) {
			auto result = ResultEnvelope::Failure("asset.reimport", resolvedTarget.generic_string(), "Failed to scan reimport target");
			result.AddDetail({ DiagnosticSeverity::Error, "asset.reimport.scan_failed", errorCode.message(), resolvedTarget.generic_string() });
			return publishReport(std::move(result));
		}

		std::sort(sourcePaths.begin(), sourcePaths.end(), [](const auto& left, const auto& right) {
			return left.generic_string() < right.generic_string();
		});
		report.ScannedFiles = static_cast<uint32_t>(sourcePaths.size());

		std::vector<ReimportCandidate> candidates;
		for (const auto& sourcePath : sourcePaths) {
			const auto match = m_ImporterRegistry.FindByExtension(sourcePath.extension().string());
			if (!match) {
				++report.SkippedFiles;
				continue;
			}

			const auto relativePath = sourcePath.lexically_relative(assetRoot).lexically_normal();
			if (relativePath.empty() || relativePath.is_absolute() || IsEscapingAssetRoot(relativePath)) {
				++report.FailedAssets;
				continue;
			}
			++report.SupportedFiles;
			candidates.push_back({
				.AssetId = relativePath.generic_string(),
				.RelativePath = relativePath,
				.Kind = match->Kind
			});
		}

		const bool singleFileTarget = sourcePaths.size() == 1 && std::filesystem::is_regular_file(resolvedTarget, errorCode);
		if (singleFileTarget && candidates.empty() && report.SkippedFiles == 1) {
			auto result = ResultEnvelope::ManualIntervention("asset.reimport", resolvedTarget.generic_string(), "No importer supports the selected asset file");
			result.AddDetail({ DiagnosticSeverity::Warning, "asset.reimport.unsupported", "The selected file extension has no registered importer", resolvedTarget.extension().string() });
			return publishReport(std::move(result));
		}

		if (m_Manifest.Empty()) {
			auto manifestResult = LoadOrCreateManifestInternal(context, false);
			if (!manifestResult.Succeeded()) {
				manifestResult.Operation = "asset.reimport";
				return publishReport(std::move(manifestResult));
			}
		}

		auto result = ResultEnvelope::Success("asset.reimport", resolvedTarget.generic_string(), "Assets reimported");
		std::vector<ReimportCandidate> importCandidates;
		for (const auto& candidate : candidates) {
			const auto* existingRecord = m_Manifest.FindByAssetId(candidate.AssetId);
			const bool isNewAsset = existingRecord == nullptr;
			if (existingRecord &&
				(existingRecord->Guid.empty() || existingRecord->Kind != candidate.Kind || existingRecord->Source != AssetSource::File)) {
				++report.FailedAssets;
				result.AddDetail({
					DiagnosticSeverity::Error,
					"asset.reimport.metadata_conflict",
					"Existing asset metadata does not match the source importer",
					candidate.AssetId
				});
				continue;
			}

			AssetManifestRecord manifestRecord;
			manifestRecord.Guid = existingRecord
				? existingRecord->Guid
				: GetExistingGuidOrGenerate(m_Registry, m_Manifest, candidate.AssetId);
			manifestRecord.AssetId = candidate.AssetId;
			manifestRecord.Kind = candidate.Kind;
			manifestRecord.Source = AssetSource::File;
			manifestRecord.RelativePath = candidate.RelativePath;
			manifestRecord.ImportState = AssetImportState::Registered;
			const auto sourcePath = assetRoot / candidate.RelativePath;
			const auto* importer = m_ImporterRegistry.Find(candidate.Kind, candidate.RelativePath.extension().string());
			AssetMeta meta;
			std::error_code metaError;
			if (std::filesystem::is_regular_file(GetAssetMetaPath(sourcePath), metaError) && !metaError) {
				auto loadMetaResult = LoadAssetMeta(sourcePath, meta);
				if (!loadMetaResult.Succeeded() || !importer || meta.Guid != manifestRecord.Guid || meta.ImporterId != importer->GetId()) {
					++report.FailedAssets;
					result.AddDetail({ DiagnosticSeverity::Error, "asset.meta.importer_mismatch", "Asset metadata conflicts with the reimport source", candidate.AssetId });
					continue;
				}
			}
			else {
				meta.Guid = manifestRecord.Guid;
				meta.ImporterId = importer ? std::string(importer->GetId()) : std::string();
				meta.SettingsVersion = importer ? importer->GetSettingsVersion() : 1;
				if (auto saveMetaResult = SaveAssetMeta(sourcePath, meta); !saveMetaResult.Succeeded()) {
					++report.FailedAssets;
					for (auto& detail : saveMetaResult.Details) result.AddDetail(std::move(detail));
					continue;
				}
			}
			if (!m_Manifest.Upsert(manifestRecord)) {
				++report.FailedAssets;
				result.AddDetail({ DiagnosticSeverity::Error, "asset.reimport.manifest_conflict", "Asset manifest rejected the source record", candidate.AssetId });
				continue;
			}

			if (m_Registry.Upsert(MakeRegistryRecord(context, manifestRecord)) == 0) {
				++report.FailedAssets;
				result.AddDetail({ DiagnosticSeverity::Error, "asset.reimport.registry_conflict", "Asset registry rejected the source record", candidate.AssetId });
				continue;
			}
			if (isNewAsset) {
				++report.RegisteredAssets;
			}
			importCandidates.push_back(candidate);
		}

		auto manifestSaveResult = SaveAssetManifest(context, m_Manifest);
		if (!manifestSaveResult.Succeeded()) {
			manifestSaveResult.Operation = "asset.reimport";
			return publishReport(std::move(manifestSaveResult));
		}

		auto libraryResult = m_Library.Open(context);
		if (!libraryResult.Succeeded()) {
			libraryResult.Operation = "asset.reimport";
			return publishReport(std::move(libraryResult));
		}

		std::vector<AssetGuid> importGuids;
		importGuids.reserve(importCandidates.size());
		for (const auto& candidate : importCandidates) {
			if (const auto* record = m_Manifest.FindByAssetId(candidate.AssetId)) {
				importGuids.push_back(record->Guid);
			}
		}

		AssetImportReport importReport;
		AssetImportService importService(m_ImporterRegistry, m_Library);
		std::unordered_set<AssetGuid> reportedReimports(importGuids.begin(), importGuids.end());
		std::vector<AssetGuid> pendingReportedDependents(importGuids.begin(), importGuids.end());
		while (!pendingReportedDependents.empty()) {
			auto guid = std::move(pendingReportedDependents.back());
			pendingReportedDependents.pop_back();
			for (auto& dependentGuid : m_Library.FindDependents(guid)) {
				if (reportedReimports.insert(dependentGuid).second) pendingReportedDependents.push_back(std::move(dependentGuid));
			}
		}
		auto importResult = importService.ImportAssets(context, m_Manifest, importGuids, AssetImportPolicy::Force, &importReport);
		for (const auto& guid : importReport.ImportedAssetGuids) m_LastImportFailures.erase(guid);
		for (const auto& failure : importReport.Failures) {
			auto diagnostics = failure.Diagnostics;
			if (diagnostics.empty()) diagnostics.push_back({ DiagnosticSeverity::Error, "asset.import.failed", "Asset import failed", failure.Guid });
			m_LastImportFailures[failure.Guid] = std::move(diagnostics);
		}
		const auto reportedReimportCount = static_cast<uint32_t>(std::count_if(importReport.ImportedAssetGuids.begin(), importReport.ImportedAssetGuids.end(), [&](const auto& guid) {
			return reportedReimports.contains(guid);
		}));
		std::vector<AssetGuid> pendingInvalidations = importReport.ImportedAssetGuids;
		std::unordered_set<AssetGuid> invalidatedGuids;
		while (!pendingInvalidations.empty()) {
			auto guid = std::move(pendingInvalidations.back());
			pendingInvalidations.pop_back();
			if (!invalidatedGuids.insert(guid).second) {
				continue;
			}
			m_RuntimeCache.Invalidate(guid);
			for (auto& dependentGuid : m_Library.FindDependents(guid)) {
				pendingInvalidations.push_back(std::move(dependentGuid));
			}
		}
		if (!importResult.Succeeded()) {
			importResult.Operation = "asset.reimport";
			report.ReimportedAssets = reportedReimportCount;
			report.FailedAssets += importReport.FailedAssets;
			return publishReport(std::move(importResult));
		}
		for (const auto& detail : importResult.Details) {
			result.AddDetail(detail);
		}
		report.ReimportedAssets = reportedReimportCount;
		report.FailedAssets += importReport.FailedAssets;
		if (report.FailedAssets > 0) {
			result.Summary = "Asset reimport completed with per-asset failures";
		}
		return publishReport(std::move(result));
	}

	ResultEnvelope AssetService::LoadManifestReadOnly(const ProjectContext& context) {
		const auto manifestPath = GetAssetManifestPath(context);
		std::error_code errorCode;
		if (!std::filesystem::is_regular_file(manifestPath, errorCode)) {
			auto result = ResultEnvelope::ManualIntervention("asset.manifest.load", manifestPath.generic_string(), "Asset manifest does not exist");
			result.AddDetail({ DiagnosticSeverity::Warning, "asset.manifest.missing", "Run asset manifest init before listing project assets", manifestPath.generic_string() });
			return result;
		}

		AssetManifest loadedManifest;
		auto result = LoadAssetManifest(context, loadedManifest);
		if (!result.Succeeded()) {
			return result;
		}

		AssetManifest builtinCatalog;
		auto catalogResult = LoadBuiltinAssetCatalog(builtinCatalog);
		if (!catalogResult.Succeeded()) {
			return catalogResult;
		}
		auto mergeResult = MergeBuiltinAssetCatalog(builtinCatalog, loadedManifest);
		if (!mergeResult.Succeeded()) {
			return mergeResult;
		}
		m_Registry = AssetRegistry();
		m_RuntimeCache = AssetRuntimeCache();
		m_Manifest = std::move(loadedManifest);
		m_Manifest.ForEachRecord([&](const AssetManifestRecord& manifestRecord) {
			(void)m_Registry.Upsert(MakeRegistryRecord(context, manifestRecord));
		});
		return ResultEnvelope::Success("asset.manifest.load", manifestPath.generic_string(), "Asset manifest loaded read-only");
	}

	ResultEnvelope AssetService::CreateBuiltinMeshAsset(
		const ProjectContext& context,
		std::string_view assetId,
		BuiltinMeshPrimitive primitive,
		std::string_view meshName,
		AssetHandle* outHandle) {
		NormalizedAssetPath normalizedPath;
		ResultEnvelope normalizeError;
		if (!TryNormalizeAssetPath(context, assetId, normalizedPath, normalizeError)) {
			normalizeError.Operation = "asset.create_builtin_mesh";
			return normalizeError;
		}

		auto mesh = CreateBuiltinMesh(primitive, meshName);
		if (!mesh) {
			auto result = ResultEnvelope::Failure("asset.create_builtin_mesh", normalizedPath.AssetId, "Unsupported built-in mesh primitive");
			result.AddDetail({ DiagnosticSeverity::Error, "asset.mesh.primitive.invalid", "The requested built-in mesh primitive is not supported", std::string(ToString(primitive)) });
			return result;
		}

		std::error_code errorCode;
		const auto parentPath = normalizedPath.AbsolutePath.parent_path();
		if (!parentPath.empty()) {
			std::filesystem::create_directories(parentPath, errorCode);
			if (errorCode) {
				auto result = ResultEnvelope::Failure("asset.create_builtin_mesh", normalizedPath.AssetId, "Failed to create mesh asset directory");
				result.AddDetail({ DiagnosticSeverity::Error, "asset.directory.create_failed", errorCode.message(), parentPath.generic_string() });
				return result;
			}
		}

		if (!Rendering::Mesh::SaveToFile(*mesh, normalizedPath.AbsolutePath.string())) {
			auto result = ResultEnvelope::Failure("asset.create_builtin_mesh", normalizedPath.AssetId, "Failed to persist built-in mesh asset");
			result.AddDetail({ DiagnosticSeverity::Error, "asset.mesh.save_failed", "Mesh::SaveToFile returned false", normalizedPath.AbsolutePath.generic_string() });
			return result;
		}

		auto result = RegisterMeshAsset(context, normalizedPath.AssetId, mesh, outHandle);
		result.Operation = "asset.create_builtin_mesh";
		result.Summary = "Built-in mesh asset created";
		result.SetPayloadValue("primitive", std::string(ToString(primitive)));
		result.SetPayloadValue("mesh_name", mesh->GetName());
		return result;
	}

	ResultEnvelope AssetService::RegisterMeshAsset(
		const ProjectContext& context,
		std::string_view assetId,
		const Ref<Rendering::Mesh>& mesh,
		AssetHandle* outHandle) {
		if (!mesh) {
			auto result = ResultEnvelope::Failure("asset.register_mesh", std::string(assetId), "Mesh asset is null");
			result.AddDetail({ DiagnosticSeverity::Error, "asset.mesh.null", "Mesh registration requires a valid mesh instance", {} });
			return result;
		}

		NormalizedAssetPath normalizedPath;
		ResultEnvelope normalizeError;
		if (!TryNormalizeAssetPath(context, assetId, normalizedPath, normalizeError)) {
			normalizeError.Operation = "asset.register_mesh";
			return normalizeError;
		}

		if (m_Manifest.Empty()) {
			auto manifestResult = LoadOrCreateManifest(context);
			if (!manifestResult.Succeeded()) {
				manifestResult.Operation = "asset.register_mesh";
				return manifestResult;
			}
		}

		AssetManifestRecord manifestRecord;
		manifestRecord.Guid = GetExistingGuidOrGenerate(m_Registry, m_Manifest, normalizedPath.AssetId);
		manifestRecord.Kind = AssetKind::Mesh;
		manifestRecord.Source = AssetSource::File;
		manifestRecord.AssetId = normalizedPath.AssetId;
		manifestRecord.RelativePath = normalizedPath.RelativePath;
		manifestRecord.ImportState = AssetImportState::Registered;
		if (!m_Manifest.Upsert(manifestRecord)) {
			return ResultEnvelope::Failure("asset.register_mesh", normalizedPath.AssetId, "Mesh manifest record conflicts with an existing asset");
		}
		auto saveResult = SaveAssetManifest(context, m_Manifest);
		if (!saveResult.Succeeded()) {
			saveResult.Operation = "asset.register_mesh";
			return saveResult;
		}

		auto record = MakeRegistryRecord(context, manifestRecord);
		record.AbsolutePath = normalizedPath.AbsolutePath;
		record.ExistsOnDisk = normalizedPath.ExistsOnDisk;
		const auto handle = m_Registry.Upsert(record);
		if (handle == 0) {
			return ResultEnvelope::Failure("asset.register_mesh", normalizedPath.AssetId, "Mesh registry record conflicts with an existing asset");
		}
		m_RuntimeCache.StoreMesh(manifestRecord.Guid, mesh);
		Rendering::MeshManager::Instance().RegisterMesh(normalizedPath.AssetId, mesh);
		if (outHandle) {
			*outHandle = handle;
		}

		return MakeRegistrationResult("asset.register_mesh", *m_Registry.Find(handle), "Mesh asset registered");
	}

	ResultEnvelope AssetService::LoadMeshAsset(
		const ProjectContext& context,
		std::string_view assetId,
		AssetHandle* outHandle) {
		NormalizedAssetPath normalizedPath;
		ResultEnvelope normalizeError;
		if (!TryNormalizeAssetPath(context, assetId, normalizedPath, normalizeError)) {
			normalizeError.Operation = "asset.load_mesh";
			return normalizeError;
		}

		std::error_code errorCode;
		if (!std::filesystem::is_regular_file(normalizedPath.AbsolutePath, errorCode)) {
			auto result = ResultEnvelope::Failure("asset.load_mesh", normalizedPath.AssetId, "Mesh asset file does not exist");
			result.AddDetail({ DiagnosticSeverity::Error, "asset.mesh.file_missing", "Mesh asset file must exist before loading", normalizedPath.AbsolutePath.generic_string() });
			return result;
		}

		const auto* importer = m_ImporterRegistry.Find(AssetKind::Mesh, normalizedPath.RelativePath.extension().string());
		if (!importer) {
			auto result = ResultEnvelope::ManualIntervention("asset.load_mesh", normalizedPath.AssetId, "Mesh asset source format is unsupported");
			result.AddDetail({ DiagnosticSeverity::Error, "asset.mesh.importer_missing", "No mesh importer supports the source extension", normalizedPath.RelativePath.extension().string() });
			return result;
		}

		AssetManifestRecord sourceRecord;
		sourceRecord.AssetId = normalizedPath.AssetId;
		sourceRecord.Kind = AssetKind::Mesh;
		sourceRecord.Source = AssetSource::File;
		sourceRecord.RelativePath = normalizedPath.RelativePath;
		sourceRecord.ImportState = AssetImportState::Registered;
		const auto settings = importer->CreateDefaultSettings();
		if (!settings || !importer->ValidateSettings(*settings).Succeeded()) {
			return ResultEnvelope::Failure("asset.load_mesh", normalizedPath.AssetId, "Mesh importer default settings are invalid");
		}
		const auto importResult = importer->Import({ context, sourceRecord, normalizedPath.AbsolutePath, &m_Manifest, &m_Library, settings.get() });
		if (!importResult.Success) {
			auto result = ResultEnvelope::ManualIntervention("asset.load_mesh", normalizedPath.AssetId, "Mesh asset source could not be imported");
			for (const auto& diagnostic : importResult.Diagnostics) {
				result.AddDetail(diagnostic);
			}
			return result;
		}

		Ref<Rendering::Mesh> mesh;
		auto decodeResult = DecodeMeshArtifact(importResult.Artifact, mesh);
		if (!decodeResult.Succeeded() || !mesh) {
			decodeResult.Operation = "asset.load_mesh";
			return decodeResult;
		}

		return RegisterMeshAsset(context, normalizedPath.AssetId, mesh, outHandle);
	}

	ResultEnvelope AssetService::RegisterMaterialAsset(
		const ProjectContext& context,
		std::string_view assetId,
		const Ref<Rendering::Material>& material,
		AssetHandle* outHandle) {
		if (!material) {
			auto result = ResultEnvelope::Failure("asset.register_material", std::string(assetId), "Material asset is null");
			result.AddDetail({ DiagnosticSeverity::Error, "asset.material.null", "Material registration requires a valid material instance", {} });
			return result;
		}

		NormalizedAssetPath normalizedPath;
		ResultEnvelope normalizeError;
		if (!TryNormalizeAssetPath(context, assetId, normalizedPath, normalizeError)) {
			normalizeError.Operation = "asset.register_material";
			return normalizeError;
		}

		if (m_Manifest.Empty()) {
			auto manifestResult = LoadOrCreateManifest(context);
			if (!manifestResult.Succeeded()) {
				manifestResult.Operation = "asset.register_material";
				return manifestResult;
			}
		}

		AssetManifestRecord manifestRecord;
		manifestRecord.Guid = GetExistingGuidOrGenerate(m_Registry, m_Manifest, normalizedPath.AssetId);
		manifestRecord.Kind = AssetKind::Material;
		manifestRecord.Source = AssetSource::File;
		manifestRecord.AssetId = normalizedPath.AssetId;
		manifestRecord.RelativePath = normalizedPath.RelativePath;
		manifestRecord.ImportState = AssetImportState::Registered;
		if (!m_Manifest.Upsert(manifestRecord)) {
			return ResultEnvelope::Failure("asset.register_material", normalizedPath.AssetId, "Material manifest record conflicts with an existing asset");
		}
		auto saveResult = SaveAssetManifest(context, m_Manifest);
		if (!saveResult.Succeeded()) {
			saveResult.Operation = "asset.register_material";
			return saveResult;
		}

		auto record = MakeRegistryRecord(context, manifestRecord);
		record.AbsolutePath = normalizedPath.AbsolutePath;
		record.ExistsOnDisk = normalizedPath.ExistsOnDisk;
		const auto handle = m_Registry.Upsert(record);
		if (handle == 0) {
			return ResultEnvelope::Failure("asset.register_material", normalizedPath.AssetId, "Material registry record conflicts with an existing asset");
		}
		m_RuntimeCache.StoreMaterial(manifestRecord.Guid, material);
		Rendering::MaterialLibrary::Instance().RegisterMaterial(normalizedPath.AssetId, material);
		if (!material->GetName().empty() && material->GetName() != normalizedPath.AssetId) {
			Rendering::MaterialLibrary::Instance().RegisterMaterial(material->GetName(), material);
		}
		if (outHandle) {
			*outHandle = handle;
		}

		return MakeRegistrationResult("asset.register_material", *m_Registry.Find(handle), "Material asset registered");
	}

	ResultEnvelope AssetService::LoadMaterialAsset(
		const ProjectContext& context,
		std::string_view assetId,
		AssetHandle* outHandle) {
		NormalizedAssetPath normalizedPath;
		ResultEnvelope normalizeError;
		if (!TryNormalizeAssetPath(context, assetId, normalizedPath, normalizeError)) {
			normalizeError.Operation = "asset.load_material";
			return normalizeError;
		}

		std::error_code errorCode;
		if (!std::filesystem::is_regular_file(normalizedPath.AbsolutePath, errorCode)) {
			auto result = ResultEnvelope::Failure("asset.load_material", normalizedPath.AssetId, "Material asset file does not exist");
			result.AddDetail({ DiagnosticSeverity::Error, "asset.material.file_missing", "Material asset file must exist before loading", normalizedPath.AbsolutePath.generic_string() });
			return result;
		}

		auto material = Rendering::Material::CreateFromDeserialization();
		if (!material || !Serialization::LoadMaterial(normalizedPath.AbsolutePath.generic_string(), *material)) {
			auto result = ResultEnvelope::ManualIntervention("asset.load_material", normalizedPath.AssetId, "Material asset file exists but could not be deserialized");
			result.AddDetail({ DiagnosticSeverity::Error, "asset.material.deserialize_failed", "Material deserialization returned false", normalizedPath.AbsolutePath.generic_string() });
			return result;
		}

		return RegisterMaterialAsset(context, normalizedPath.AssetId, material, outHandle);
	}

	ResultEnvelope AssetService::RegisterTextureAsset(
		const ProjectContext& context,
		std::string_view assetId,
		const Ref<Rendering::TextureResource>& texture,
		AssetHandle* outHandle) {
		NormalizedAssetPath normalizedPath;
		ResultEnvelope normalizeError;
		if (!TryNormalizeAssetPath(context, assetId, normalizedPath, normalizeError)) {
			normalizeError.Operation = "asset.register_texture";
			return normalizeError;
		}

		if (m_Manifest.Empty()) {
			auto manifestResult = LoadOrCreateManifest(context);
			if (!manifestResult.Succeeded()) {
				manifestResult.Operation = "asset.register_texture";
				return manifestResult;
			}
		}

		AssetRecord record;
		record.Guid = GetExistingGuidOrGenerate(m_Registry, m_Manifest, normalizedPath.AssetId);
		record.Kind = AssetKind::Texture2D;
		record.Source = AssetSource::File;
		record.AssetId = normalizedPath.AssetId;
		record.RelativePath = normalizedPath.RelativePath;
		record.AbsolutePath = normalizedPath.AbsolutePath;
		std::error_code errorCode;
		record.ExistsOnDisk = std::filesystem::is_regular_file(normalizedPath.AbsolutePath, errorCode);
		record.ImportState = record.ExistsOnDisk ? AssetImportState::Registered : AssetImportState::Missing;

		if (!record.ExistsOnDisk) {
			auto result = ResultEnvelope::ManualIntervention("asset.register_texture", normalizedPath.AssetId, "Texture asset is unresolved");
			result.SetPayloadValue("asset_id", normalizedPath.AssetId);
			result.SetPayloadValue("asset_kind", std::string(ToString(AssetKind::Texture2D)));
			result.SetPayloadValue("asset_path", normalizedPath.AbsolutePath.generic_string());
			result.SetPayloadValue("exists_on_disk", "false");
			result.AddDetail({ DiagnosticSeverity::Warning, "asset.texture.unresolved", "Task 1 texture registration requires an existing source file; runtime-only texture payloads are not stored in metadata registry", normalizedPath.AbsolutePath.generic_string() });
			return result;
		}

		AssetManifestRecord manifestRecord;
		manifestRecord.Guid = record.Guid;
		manifestRecord.Kind = record.Kind;
		manifestRecord.Source = record.Source;
		manifestRecord.AssetId = record.AssetId;
		manifestRecord.RelativePath = record.RelativePath;
		manifestRecord.ImportState = record.ImportState;
		if (!m_Manifest.Upsert(manifestRecord)) {
			return ResultEnvelope::Failure("asset.register_texture", normalizedPath.AssetId, "Texture manifest record conflicts with an existing asset");
		}
		auto saveResult = SaveAssetManifest(context, m_Manifest);
		if (!saveResult.Succeeded()) {
			saveResult.Operation = "asset.register_texture";
			return saveResult;
		}

		const auto handle = m_Registry.Upsert(record);
		if (handle == 0) {
			return ResultEnvelope::Failure("asset.register_texture", normalizedPath.AssetId, "Texture registry record conflicts with an existing asset");
		}
		if (texture) {
			m_RuntimeCache.StoreTexture(record.Guid, texture);
		}
		if (outHandle) {
			*outHandle = handle;
		}

		return MakeRegistrationResult("asset.register_texture", *m_Registry.Find(handle), "Texture asset registered");
	}

	ResultEnvelope AssetService::RegisterShaderAsset(
		const ProjectContext& context,
		std::string_view assetId,
		AssetHandle* outHandle) {
		NormalizedAssetPath normalizedPath;
		ResultEnvelope normalizeError;
		if (!TryNormalizeAssetPath(context, assetId, normalizedPath, normalizeError)) {
			normalizeError.Operation = "asset.register_shader";
			return normalizeError;
		}
		if (!m_ImporterRegistry.Find(AssetKind::Shader, normalizedPath.RelativePath.extension().string())) {
			return ResultEnvelope::Failure("asset.register_shader", normalizedPath.AssetId, "Shader source extension is unsupported");
		}
		std::error_code errorCode;
		if (!std::filesystem::is_regular_file(normalizedPath.AbsolutePath, errorCode)) {
			auto result = ResultEnvelope::ManualIntervention("asset.register_shader", normalizedPath.AssetId, "Shader asset is unresolved");
			result.AddDetail({ DiagnosticSeverity::Warning, "asset.shader.unresolved", "Shader registration requires an existing source file", normalizedPath.AbsolutePath.generic_string() });
			return result;
		}

		if (m_Manifest.Empty()) {
			auto manifestResult = LoadOrCreateManifest(context);
			if (!manifestResult.Succeeded()) {
				manifestResult.Operation = "asset.register_shader";
				return manifestResult;
			}
		}

		AssetRecord record;
		record.Guid = GetExistingGuidOrGenerate(m_Registry, m_Manifest, normalizedPath.AssetId);
		record.Kind = AssetKind::Shader;
		record.Source = AssetSource::File;
		record.AssetId = normalizedPath.AssetId;
		record.RelativePath = normalizedPath.RelativePath;
		record.AbsolutePath = normalizedPath.AbsolutePath;
		record.ExistsOnDisk = true;
		record.ImportState = AssetImportState::Registered;

		AssetManifestRecord manifestRecord;
		manifestRecord.Guid = record.Guid;
		manifestRecord.Kind = record.Kind;
		manifestRecord.Source = record.Source;
		manifestRecord.AssetId = record.AssetId;
		manifestRecord.RelativePath = record.RelativePath;
		manifestRecord.ImportState = record.ImportState;
		if (!m_Manifest.Upsert(manifestRecord)) {
			return ResultEnvelope::Failure("asset.register_shader", normalizedPath.AssetId, "Shader manifest record conflicts with an existing asset");
		}
		auto saveResult = SaveAssetManifest(context, m_Manifest);
		if (!saveResult.Succeeded()) {
			saveResult.Operation = "asset.register_shader";
			return saveResult;
		}

		const auto handle = m_Registry.Upsert(record);
		if (handle == 0) {
			return ResultEnvelope::Failure("asset.register_shader", normalizedPath.AssetId, "Shader registry record conflicts with an existing asset");
		}
		if (outHandle) *outHandle = handle;
		return MakeRegistrationResult("asset.register_shader", *m_Registry.Find(handle), "Shader asset registered");
	}

	ResultEnvelope AssetService::ResolveAsset(AssetHandle handle, AssetRecord& outRecord) const {
		const auto* record = m_Registry.Find(handle);
		if (!record) {
			return MakeResolveFailure("asset.resolve", HandleToString(handle), "Asset handle was not found");
		}

		outRecord = *record;
		auto result = ResultEnvelope::Success("asset.resolve", HandleToString(handle), "Asset resolved by handle");
		result.SetPayloadValue("asset_handle", HandleToString(record->Handle));
		result.SetPayloadValue("asset_id", record->AssetId);
		result.SetPayloadValue("asset_kind", std::string(ToString(record->Kind)));
		return result;
	}

	ResultEnvelope AssetService::ResolveAsset(std::string_view assetId, AssetRecord& outRecord) const {
		const auto* record = m_Registry.Find(assetId);
		if (!record) {
			return MakeResolveFailure("asset.resolve", std::string(assetId), "Asset id was not found");
		}

		outRecord = *record;
		auto result = ResultEnvelope::Success("asset.resolve", record->AssetId, "Asset resolved by id");
		result.SetPayloadValue("asset_handle", HandleToString(record->Handle));
		result.SetPayloadValue("asset_kind", std::string(ToString(record->Kind)));
		return result;
	}

	ResultEnvelope AssetService::ResolveMeshAsset(AssetHandle handle, Ref<Rendering::Mesh>& outMesh) const {
		const auto* record = m_Registry.Find(handle);
		if (!record) {
			return MakeResolveFailure("asset.resolve_mesh", HandleToString(handle), "Mesh asset handle was not found");
		}

		if (record->Kind != AssetKind::Mesh) {
			auto result = ResultEnvelope::Failure("asset.resolve_mesh", HandleToString(handle), "Asset handle is not a mesh asset");
			result.AddDetail({ DiagnosticSeverity::Error, "asset.kind.mismatch", "Requested mesh resolve for a non-mesh asset", std::string(ToString(record->Kind)) });
			return result;
		}

		AssetResolver resolver(const_cast<AssetService&>(*this));
		auto result = resolver.ResolveMesh(record->Guid, outMesh);
		result.Target = HandleToString(handle);
		result.SetPayloadValue("asset_id", record->AssetId);
		return result;
	}

	ResultEnvelope AssetService::ResolveMaterialAsset(AssetHandle handle, Ref<Rendering::Material>& outMaterial) const {
		const auto* record = m_Registry.Find(handle);
		if (!record) {
			return MakeResolveFailure("asset.resolve_material", HandleToString(handle), "Material asset handle was not found");
		}

		if (record->Kind != AssetKind::Material) {
			auto result = ResultEnvelope::Failure("asset.resolve_material", HandleToString(handle), "Asset handle is not a material asset");
			result.AddDetail({ DiagnosticSeverity::Error, "asset.kind.mismatch", "Requested material resolve for a non-material asset", std::string(ToString(record->Kind)) });
			return result;
		}

		AssetResolver resolver(const_cast<AssetService&>(*this));
		auto result = resolver.ResolveMaterial(record->Guid, outMaterial);
		result.Target = HandleToString(handle);
		result.SetPayloadValue("asset_id", record->AssetId);
		return result;
	}

	ResultEnvelope AssetService::ResolveTextureAsset(AssetHandle handle, Ref<Rendering::TextureResource>& outTexture) const {
		const auto* record = m_Registry.Find(handle);
		if (!record) {
			return MakeResolveFailure("asset.resolve_texture", HandleToString(handle), "Texture asset handle was not found");
		}

		if (record->Kind != AssetKind::Texture2D) {
			auto result = ResultEnvelope::Failure("asset.resolve_texture", HandleToString(handle), "Asset handle is not a texture asset");
			result.AddDetail({ DiagnosticSeverity::Error, "asset.kind.mismatch", "Requested texture resolve for a non-texture asset", std::string(ToString(record->Kind)) });
			return result;
		}

		AssetResolver resolver(const_cast<AssetService&>(*this));
		auto result = resolver.ResolveTexture(record->Guid, outTexture);
		result.Target = HandleToString(handle);
		result.SetPayloadValue("asset_id", record->AssetId);
		return result;
	}

	ResultEnvelope AssetService::ResolveShaderAsset(AssetHandle handle, Ref<Rendering::ShaderProgram>& outShader) const {
		const auto* record = m_Registry.Find(handle);
		if (!record) {
			return MakeResolveFailure("asset.resolve_shader", HandleToString(handle), "Shader asset handle was not found");
		}
		if (record->Kind != AssetKind::Shader) {
			auto result = ResultEnvelope::Failure("asset.resolve_shader", HandleToString(handle), "Asset handle is not a shader asset");
			result.AddDetail({ DiagnosticSeverity::Error, "asset.kind.mismatch", "Requested shader resolve for a non-shader asset", std::string(ToString(record->Kind)) });
			return result;
		}

		AssetResolver resolver(const_cast<AssetService&>(*this));
		auto result = resolver.ResolveShader(record->Guid, outShader);
		result.Target = HandleToString(handle);
		result.SetPayloadValue("asset_id", record->AssetId);
		return result;
	}

	const AssetRecord* AssetService::FindRecordByGuid(const AssetGuid& guid) const {
		return m_Registry.FindByGuid(guid);
	}

	ResultEnvelope AssetService::ValidateRegistry(const ProjectContext& context, AssetValidationReport* outReport) {
		AssetValidationReport report;
		if (!context.IsLoaded()) {
			auto result = ResultEnvelope::Failure("asset.validate", {}, "Project context is not loaded");
			result.AddDetail({ DiagnosticSeverity::Error, "asset.project.unloaded", "Asset validation requires a loaded project context", {} });
			if (outReport) {
				*outReport = report;
			}
			return result;
		}

		std::error_code errorCode;
		const auto assetRoot = std::filesystem::absolute(context.GetAssetRootPath(), errorCode).lexically_normal();
		if (errorCode) {
			auto result = ResultEnvelope::Failure("asset.validate", context.GetTargetId(), "Failed to resolve project asset root");
			result.AddDetail({ DiagnosticSeverity::Error, "asset.root.resolve_failed", errorCode.message(), context.GetAssetRootPath().generic_string() });
			if (outReport) {
				*outReport = report;
			}
			return result;
		}
		auto libraryResult = m_Library.Open(context);
		if (!libraryResult.Succeeded()) {
			libraryResult.Operation = "asset.validate";
			if (outReport) *outReport = report;
			return libraryResult;
		}

		m_Registry.ForEachRecord([&](const AssetRecord& record) {
			++report.TotalAssets;

			bool hasMetadataBlockingIssue = false;
			if (record.Source == AssetSource::File && IsOutsideAssetRoot(assetRoot, record.AbsolutePath.lexically_normal())) {
				++report.AssetsOutsideProjectRoot;
				hasMetadataBlockingIssue = true;
			}

			if (!record.IsOperational()) {
				++report.InvalidAssetRecords;
				hasMetadataBlockingIssue = true;
			}
			if (record.Source == AssetSource::File && !record.ExistsOnDisk) {
				++report.MissingFileAssets;
				hasMetadataBlockingIssue = true;
			}
			if (record.Source == AssetSource::Builtin) {
				if (!IsSafeAssetRelativePath(record.RelativePath) || record.AssetId.rfind("builtin/", 0) != 0) {
					++report.BuiltinMetadataIssues;
					hasMetadataBlockingIssue = true;
				}
				else if (!record.ExistsOnDisk) {
					++report.MissingBuiltinAssets;
					hasMetadataBlockingIssue = true;
				}
			}
			if (record.Guid == BuiltinAssetGuids::FallbackMesh || record.Guid == BuiltinAssetGuids::FallbackMaterial) {
				++report.FallbackAssets;
			}
			if ((record.Source == AssetSource::File || record.Source == AssetSource::Builtin) && record.Kind != AssetKind::Scene && !hasMetadataBlockingIssue) {
				const auto* importer = m_ImporterRegistry.Find(record.Kind, record.RelativePath.extension().string());
				if (!importer) {
					if (record.Source == AssetSource::Builtin) {
						++report.BuiltinAssetsWithoutImporter;
					}
					else {
						++report.FileAssetsWithoutImporter;
					}
				}
				else if (!m_Library.IsArtifactAvailable(
					record.Guid,
					record.Kind,
					importer->GetId(),
					importer->GetVersion(),
					importer->GetArtifactVersion())) {
					if (record.Source == AssetSource::Builtin) {
						++report.BuiltinAssetsMissingArtifacts;
					}
					else {
						++report.FileAssetsMissingArtifacts;
					}
				}
			}

			switch (record.Kind) {
			case AssetKind::Mesh:
				++report.MeshAssets;
				break;
			case AssetKind::Material:
				++report.MaterialAssets;
				break;
			case AssetKind::Texture2D:
				++report.TextureAssets;
				break;
			case AssetKind::Shader:
				++report.ShaderAssets;
				break;
			case AssetKind::Scene:
				++report.SceneAssets;
				break;
			case AssetKind::Unknown:
			default:
				++report.UnknownKindAssets;
				hasMetadataBlockingIssue = true;
				break;
			}
		});

		if (outReport) {
			*outReport = report;
		}

		auto result = report.IsOperational()
			? ResultEnvelope::Success("asset.validate", context.GetTargetId(), "Asset registry is operational")
			: ResultEnvelope::ManualIntervention("asset.validate", context.GetTargetId(), "Asset registry requires intervention");
		result.SetPayloadValue("asset_count", CountToString(report.TotalAssets));
		result.SetPayloadValue("metadata_issue_count", CountToString(report.MetadataIssueCount()));
		result.SetPayloadValue("runtime_issue_count", CountToString(report.RuntimeIssueCount()));
		result.SetPayloadValue("fallback_asset_count", CountToString(report.FallbackAssets));
		result.SetPayloadValue("mesh_asset_count", CountToString(report.MeshAssets));
		result.SetPayloadValue("material_asset_count", CountToString(report.MaterialAssets));
		result.SetPayloadValue("texture_asset_count", CountToString(report.TextureAssets));
		result.SetPayloadValue("shader_asset_count", CountToString(report.ShaderAssets));
		result.SetPayloadValue("scene_asset_count", CountToString(report.SceneAssets));
		result.SetPayloadValue("unknown_kind_asset_count", CountToString(report.UnknownKindAssets));
		result.SetPayloadValue("invalid_asset_record_count", CountToString(report.InvalidAssetRecords));
		result.SetPayloadValue("assets_outside_project_root", CountToString(report.AssetsOutsideProjectRoot));
		result.SetPayloadValue("missing_file_asset_count", CountToString(report.MissingFileAssets));
		result.SetPayloadValue("missing_builtin_asset_count", CountToString(report.MissingBuiltinAssets));
		result.SetPayloadValue("builtin_metadata_issue_count", CountToString(report.BuiltinMetadataIssues));
		result.SetPayloadValue("file_assets_missing_artifact_count", CountToString(report.FileAssetsMissingArtifacts));
		result.SetPayloadValue("file_assets_without_importer_count", CountToString(report.FileAssetsWithoutImporter));
		result.SetPayloadValue("builtin_assets_missing_artifact_count", CountToString(report.BuiltinAssetsMissingArtifacts));
		result.SetPayloadValue("builtin_assets_without_importer_count", CountToString(report.BuiltinAssetsWithoutImporter));

		if (report.UnknownKindAssets > 0) {
			result.AddDetail({ DiagnosticSeverity::Error, "asset.kind.unknown", "One or more asset records have an unknown asset kind", CountToString(report.UnknownKindAssets) });
		}
		if (report.InvalidAssetRecords > 0) {
			result.AddDetail({ DiagnosticSeverity::Error, "asset.record.invalid", "One or more asset records are missing both disk and runtime backing", CountToString(report.InvalidAssetRecords) });
		}
		if (report.AssetsOutsideProjectRoot > 0) {
			result.AddDetail({ DiagnosticSeverity::Error, "asset.path.outside_root", "One or more asset records resolve outside the project asset root", CountToString(report.AssetsOutsideProjectRoot) });
		}
		if (report.MissingFileAssets > 0) {
			result.AddDetail({ DiagnosticSeverity::Error, "asset.file.missing", "One or more file asset records are missing on disk", CountToString(report.MissingFileAssets) });
		}
		if (report.MissingBuiltinAssets > 0) {
			result.AddDetail({ DiagnosticSeverity::Error, "asset.builtin.missing", "One or more builtin asset source files are missing", CountToString(report.MissingBuiltinAssets) });
		}
		if (report.BuiltinMetadataIssues > 0) {
			result.AddDetail({ DiagnosticSeverity::Error, "asset.builtin.invalid", "One or more builtin asset records have invalid source metadata", CountToString(report.BuiltinMetadataIssues) });
		}
		if (report.FileAssetsMissingArtifacts > 0) {
			result.AddDetail({ DiagnosticSeverity::Warning, "asset.artifact.missing", "One or more file assets are missing compatible Library artifacts", CountToString(report.FileAssetsMissingArtifacts) });
		}
		if (report.FileAssetsWithoutImporter > 0) {
			result.AddDetail({ DiagnosticSeverity::Warning, "asset.importer.missing", "One or more file assets have no importer for their kind and extension", CountToString(report.FileAssetsWithoutImporter) });
		}
		if (report.BuiltinAssetsMissingArtifacts > 0) {
			result.AddDetail({ DiagnosticSeverity::Warning, "asset.builtin_artifact.missing", "One or more builtin assets are missing compatible Library artifacts", CountToString(report.BuiltinAssetsMissingArtifacts) });
		}
		if (report.BuiltinAssetsWithoutImporter > 0) {
			result.AddDetail({ DiagnosticSeverity::Error, "asset.builtin_importer.missing", "One or more builtin assets have no importer for their kind and extension", CountToString(report.BuiltinAssetsWithoutImporter) });
		}

		return result;
	}
}
