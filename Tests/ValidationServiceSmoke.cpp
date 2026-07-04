#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "HuaEngine.h"
#include "HuaEngine/Asset/AssetService.h"
#include "HuaEngine/Project/ProjectService.h"
#include "HuaEngine/Scene/SceneService.h"
#include "HuaEngine/Validation/ValidationService.h"

namespace {
	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[ValidationServiceSmoke] " << message << std::endl;
			std::exit(1);
		}
	}

	void RequirePayloadValue(const HE::ResultEnvelope& result, const std::string& key, const std::string& expectedValue) {
		const auto it = result.Payload.find(key);
		Require(it != result.Payload.end(), "Expected payload key '" + key + "' to exist");
		Require(it->second == expectedValue, "Expected payload key '" + key + "' to equal '" + expectedValue + "'");
	}
}

int main() {
	HE::Log::Init();
	HE::Serialization::InitializeSerialization();

	const auto smokeRoot = std::filesystem::temp_directory_path() / "HuaEngineValidationServiceSmoke";
	std::error_code errorCode;
	std::filesystem::remove_all(smokeRoot, errorCode);

	HE::ProjectService projectService;
	HE::ProjectContext projectContext;
	auto initializeProject = projectService.InitializeProject(smokeRoot / "SmokeProject", &projectContext, "SmokeProject");
	Require(initializeProject.Succeeded(), "Expected project.initialize to succeed for validation smoke");

	HE::Scene scene("ValidationScene");

	auto primaryEntity = scene.GetWorld().CreateEntity("Validation Entity");
	primaryEntity.AddComponent<HE::TransformComponent>();
	HE::MeshAssetRef meshReference;
	meshReference.Reference.Guid = HE::BuiltinAssetGuids::QuadMesh;
	primaryEntity.AddComponent<HE::MeshComponent>(meshReference);
	primaryEntity.AddComponent<HE::MaterialComponent>();

	HE::AssetService assetService;
	HE::AssetHandle meshHandle = 0;
	const auto runtimeMesh = HE::Mesh::CreateQuad("ValidationQuad");
	Require(static_cast<bool>(runtimeMesh), "Expected runtime mesh creation to succeed");
	const auto meshAssetPath = projectContext.GetAssetRootPath() / "Meshes" / "ValidationQuad.mesh";
	std::filesystem::create_directories(meshAssetPath.parent_path(), errorCode);
	Require(!errorCode, "Expected validation mesh asset directory creation to succeed");
	Require(HE::Mesh::SaveToFile(*runtimeMesh, meshAssetPath.generic_string()), "Expected validation mesh asset file save to succeed");
	auto registerMeshResult = assetService.RegisterMeshAsset(projectContext, "Meshes/ValidationQuad.mesh", runtimeMesh, &meshHandle);
	Require(registerMeshResult.Succeeded(), "Expected mesh asset registration to succeed");

	HE::AssetHandle materialHandle = 0;
	const auto runtimeMaterial = HE::Rendering::Material::Create("ValidationMaterial", HE::Rendering::MaterialType::Unlit);
	Require(static_cast<bool>(runtimeMaterial), "Expected runtime material creation to succeed");
	const auto materialAssetPath = projectContext.GetAssetRootPath() / "Materials" / "ValidationMaterial.mat";
	std::filesystem::create_directories(materialAssetPath.parent_path(), errorCode);
	Require(!errorCode, "Expected validation material asset directory creation to succeed");
	Require(HE::Serialization::SaveMaterial(*runtimeMaterial, materialAssetPath.generic_string()), "Expected validation material asset file save to succeed");
	auto registerMaterialResult = assetService.RegisterMaterialAsset(projectContext, "Materials/ValidationMaterial.mat", runtimeMaterial, &materialHandle);
	Require(registerMaterialResult.Succeeded(), "Expected material asset registration to succeed");

	HE::SceneService sceneService;
	HE::ValidationService validationService(projectService, sceneService);
	HE::ValidationRequest validationRequest;
	validationRequest.Project = &projectContext;
	validationRequest.SceneTarget = &scene;
	validationRequest.Assets = &assetService;

	HE::ValidationReport healthyReport;
	auto healthyValidation = validationService.Validate(validationRequest, &healthyReport);
	Require(healthyValidation.Succeeded(), "Expected healthy validation request to succeed");
	Require(healthyReport.DomainCount == 3, "Expected validation report to cover three domains");
	Require(healthyReport.SuccessCount == 3, "Expected all three domains to validate successfully");
	Require(healthyReport.ManualInterventionCount == 0, "Expected no domains to require manual intervention on the healthy path");
	Require(healthyReport.AssetStatus.TotalAssets == 8, "Expected asset validation to see builtin and registered assets");
	Require(healthyReport.AssetStatus.MetadataIssueCount() == 0, "Expected healthy asset validation to report no metadata issues");
	Require(healthyReport.AssetStatus.RuntimeIssueCount() == 0, "Expected healthy asset validation to report no runtime issues");
	Require(healthyReport.AssetStatus.FallbackAssets == 2, "Expected asset validation to count fallback builtin assets");
	RequirePayloadValue(healthyValidation, "project_status", "success");
	RequirePayloadValue(healthyValidation, "scene_status", "success");
	RequirePayloadValue(healthyValidation, "asset_status", "success");
	RequirePayloadValue(healthyValidation, "asset_count", "8");
	RequirePayloadValue(healthyValidation, "metadata_issue_count", "0");
	RequirePayloadValue(healthyValidation, "runtime_issue_count", "0");
	RequirePayloadValue(healthyValidation, "fallback_asset_count", "2");
	RequirePayloadValue(healthyValidation, "can_continue_automatically", "true");

	HE::AssetService missingFileAssetService;
	Require(missingFileAssetService.LoadOrCreateManifest(projectContext).Succeeded(), "Expected missing-file asset service manifest initialization to succeed");
	HE::AssetRecord missingCachedMeshRecord;
	missingCachedMeshRecord.Guid = "missing-cached-validation-mesh";
	missingCachedMeshRecord.Kind = HE::AssetKind::Mesh;
	missingCachedMeshRecord.Source = HE::AssetSource::File;
	missingCachedMeshRecord.AssetId = "Meshes/MissingCachedValidation.mesh";
	missingCachedMeshRecord.RelativePath = std::filesystem::path("Meshes/MissingCachedValidation.mesh");
	missingCachedMeshRecord.AbsolutePath = projectContext.GetAssetRootPath() / missingCachedMeshRecord.RelativePath;
	missingCachedMeshRecord.ImportState = HE::AssetImportState::Registered;
	missingCachedMeshRecord.ExistsOnDisk = false;
	Require(missingFileAssetService.GetAssetRegistry().Upsert(missingCachedMeshRecord) != 0, "Expected missing cached mesh record insertion to succeed");
	missingFileAssetService.GetRuntimeCache().StoreMesh(missingCachedMeshRecord.Guid, runtimeMesh);

	HE::ValidationRequest missingFileRequest = validationRequest;
	missingFileRequest.Assets = &missingFileAssetService;
	HE::ValidationReport missingFileReport;
	auto missingFileValidation = validationService.Validate(missingFileRequest, &missingFileReport);
	Require(missingFileValidation.RequiresManualIntervention(), "Expected missing file asset validation to require manual intervention");
	Require(missingFileReport.AssetStatus.MissingFileAssets == 1, "Expected missing file asset validation to count missing files");
	Require(missingFileReport.AssetStatus.MetadataIssueCount() == 1, "Expected missing file asset validation to report one metadata issue");
	Require(missingFileReport.AssetStatus.RuntimeIssueCount() == 0, "Expected cached missing file asset to skip runtime issue counting");
	RequirePayloadValue(missingFileValidation, "metadata_issue_count", "1");
	RequirePayloadValue(missingFileValidation, "runtime_issue_count", "0");

	primaryEntity.RemoveComponent<HE::TransformComponent>();

	HE::AssetRecord invalidAssetRecord;
	invalidAssetRecord.Guid = "invalid-validation-asset";
	invalidAssetRecord.Kind = HE::AssetKind::Unknown;
	invalidAssetRecord.Source = HE::AssetSource::File;
	invalidAssetRecord.AssetId = "Broken/Invalid.asset";
	invalidAssetRecord.RelativePath = std::filesystem::path("Broken/Invalid.asset");
	invalidAssetRecord.AbsolutePath = projectContext.GetAssetRootPath() / invalidAssetRecord.RelativePath;
	invalidAssetRecord.ExistsOnDisk = false;
	const auto invalidAssetHandle = assetService.GetAssetRegistry().Upsert(invalidAssetRecord);
	Require(invalidAssetHandle != 0, "Expected invalid asset record insertion to produce a handle for degradation coverage");

	HE::ValidationReport degradedReport;
	auto degradedValidation = validationService.Validate(validationRequest, &degradedReport);
	Require(degradedValidation.RequiresManualIntervention(), "Expected degraded validation request to require manual intervention");
	Require(degradedReport.SuccessCount == 1, "Expected only the project domain to remain successful on the degraded path");
	Require(degradedReport.ManualInterventionCount == 2, "Expected scene and asset domains to require manual intervention");
	Require(degradedReport.SceneStatus.EntitiesMissingTransform == 1, "Expected degraded scene validation to detect one missing transform");
	Require(degradedReport.AssetStatus.UnknownKindAssets == 1, "Expected degraded asset validation to detect one unknown-kind asset");
	Require(degradedReport.AssetStatus.MetadataIssueCount() == 3, "Expected degraded asset validation to report metadata issues");
	RequirePayloadValue(degradedValidation, "scene_status", "manual_intervention_required");
	RequirePayloadValue(degradedValidation, "asset_status", "manual_intervention_required");
	RequirePayloadValue(degradedValidation, "metadata_issue_count", "3");
	RequirePayloadValue(degradedValidation, "runtime_issue_count", "0");
	RequirePayloadValue(degradedValidation, "can_continue_automatically", "false");

	HE::ValidationReport emptyReport;
	auto emptyValidation = validationService.Validate({}, &emptyReport);
	Require(emptyValidation.Failed(), "Expected empty validation request to fail");

	std::filesystem::remove_all(smokeRoot, errorCode);
	Require(!errorCode, "Expected validation smoke temporary directory cleanup to succeed");

	std::cout << "ValidationServiceSmoke passed" << std::endl;
	return 0;
}
