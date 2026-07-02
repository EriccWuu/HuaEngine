#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "HuaEngine.h"
#include "HuaEngine/Asset/AssetService.h"
#include "HuaEngine/Project/ProjectService.h"
#include "HuaEngine/Scene/SceneService.h"
#include "HuaEngine/Script/ScriptService.h"
#include "HuaEngine/Script/ScriptRuntimeSystem.h"
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

	struct ValidationSmokeScript final : HE::ScriptableEntity {
	};
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
	HE::ScriptService scriptService;
	scene.AddSystem(HE::CreateRef<HE::ScriptRuntimeSystem>(scene, scriptService));

	auto primaryEntity = scene.GetWorld().CreateEntity("Validation Entity");
	primaryEntity.AddComponent<HE::TransformComponent>();
	primaryEntity.AddComponent<HE::MeshComponent>("ValidationMesh");
	primaryEntity.AddComponent<HE::MaterialComponent>();
	auto bindScriptResult = scriptService.BindNativeScript<ValidationSmokeScript>(primaryEntity, "ValidationSmokeScript");
	Require(bindScriptResult.Succeeded(), "Expected script.bind to succeed for validation smoke");

	HE::AssetService assetService;
	HE::AssetHandle meshHandle = 0;
	const auto runtimeMesh = HE::Mesh::CreateQuad("ValidationQuad");
	Require(static_cast<bool>(runtimeMesh), "Expected runtime mesh creation to succeed");
	auto registerMeshResult = assetService.RegisterMeshAsset(projectContext, "Meshes/ValidationQuad.mesh", runtimeMesh, &meshHandle);
	Require(registerMeshResult.Succeeded(), "Expected mesh asset registration to succeed");

	HE::AssetHandle materialHandle = 0;
	const auto runtimeMaterial = HE::Rendering::Material::Create("ValidationMaterial", HE::Rendering::MaterialType::Unlit);
	Require(static_cast<bool>(runtimeMaterial), "Expected runtime material creation to succeed");
	auto registerMaterialResult = assetService.RegisterMaterialAsset(projectContext, "Materials/ValidationMaterial.mat", runtimeMaterial, &materialHandle);
	Require(registerMaterialResult.Succeeded(), "Expected material asset registration to succeed");

	HE::SceneService sceneService;
	HE::ValidationService validationService(projectService, sceneService);
	HE::ValidationRequest validationRequest;
	validationRequest.Project = &projectContext;
	validationRequest.SceneTarget = &scene;
	validationRequest.Assets = &assetService;
	validationRequest.ScriptScene = &scene;
	validationRequest.Scripts = &scriptService;

	HE::ValidationReport healthyReport;
	auto healthyValidation = validationService.Validate(validationRequest, &healthyReport);
	Require(healthyValidation.Succeeded(), "Expected healthy validation request to succeed");
	Require(healthyReport.DomainCount == 4, "Expected validation report to cover four domains");
	Require(healthyReport.SuccessCount == 4, "Expected all four domains to validate successfully");
	Require(healthyReport.ManualInterventionCount == 0, "Expected no domains to require manual intervention on the healthy path");
	Require(healthyReport.AssetStatus.TotalAssets == 2, "Expected asset validation to see two registered assets");
	RequirePayloadValue(healthyValidation, "project_status", "success");
	RequirePayloadValue(healthyValidation, "scene_status", "success");
	RequirePayloadValue(healthyValidation, "asset_status", "success");
	RequirePayloadValue(healthyValidation, "script_status", "success");
	RequirePayloadValue(healthyValidation, "can_continue_automatically", "true");

	auto invalidScriptEntity = scene.GetWorld().CreateEntity("Invalid Script Entity");
	invalidScriptEntity.AddComponent<HE::TransformComponent>();
	invalidScriptEntity.AddComponent<HE::NativeScriptComponent>();
	primaryEntity.RemoveComponent<HE::TransformComponent>();

	HE::AssetRecord invalidAssetRecord;
	invalidAssetRecord.Kind = HE::AssetKind::Unknown;
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
	Require(degradedReport.ManualInterventionCount == 3, "Expected scene, asset, and script domains to require manual intervention");
	Require(degradedReport.SceneStatus.EntitiesMissingTransform == 1, "Expected degraded scene validation to detect one missing transform");
	Require(degradedReport.AssetStatus.UnknownKindAssets == 1, "Expected degraded asset validation to detect one unknown-kind asset");
	Require(degradedReport.ScriptStatus.MissingBindingComponents == 1, "Expected degraded script validation to detect one missing binding");
	RequirePayloadValue(degradedValidation, "scene_status", "manual_intervention_required");
	RequirePayloadValue(degradedValidation, "asset_status", "manual_intervention_required");
	RequirePayloadValue(degradedValidation, "script_status", "manual_intervention_required");
	RequirePayloadValue(degradedValidation, "can_continue_automatically", "false");

	HE::ValidationReport emptyReport;
	auto emptyValidation = validationService.Validate({}, &emptyReport);
	Require(emptyValidation.Failed(), "Expected empty validation request to fail");

	std::filesystem::remove_all(smokeRoot, errorCode);
	Require(!errorCode, "Expected validation smoke temporary directory cleanup to succeed");

	std::cout << "ValidationServiceSmoke passed" << std::endl;
	return 0;
}
