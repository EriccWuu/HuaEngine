#include <cstdlib>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "HuaEngine.h"
#include "HuaEngine/Asset/AssetInspection.h"
#include "HuaEngine/Application/ApplicationServices.h"
#include "HuaEngine/Asset/Import/AssetImportService.h"
#include "Support/TestTextureFixture.h"

namespace {
	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[ApplicationOperationsSmoke] " << message << std::endl;
			std::exit(1);
		}
	}

	HE::ApplicationSpecification MakeApplicationSpecification() {
		HE::ApplicationSpecification specification;
		specification.Name = "ApplicationOperationsSmoke";
		specification.EnableGuiLayer = false;
		return specification;
	}

	class SmokeApplication final : public HE::Application {
	public:
		SmokeApplication()
			: HE::Application(MakeApplicationSpecification()) {}

		HE::ApplicationServices& Services() { return GetServices(); }
	};
}

int main() {
	HE::Log::Init();
	SmokeApplication application;
	application.Start();

	auto& operations = application.GetOperations();

	Require(operations.Supports("project.initialize"), "Expected project.initialize to be published through the operation registry");
	Require(operations.Supports("scene.create"), "Expected scene.create to be published through the operation registry");
	Require(operations.Supports("scene.entity.create"), "Expected scene.entity.create to be published through the operation registry");
	Require(operations.Supports("scene.component.add"), "Expected scene.component.add to be published through the operation registry");
	Require(operations.Supports("asset.manifest.init"), "Expected asset.manifest.init to be published");
	Require(operations.Supports("asset.initialize"), "Expected asset.initialize to be published");
	Require(operations.Supports("asset.import"), "Expected asset.import to be published");
	Require(operations.Supports("asset.reimport"), "Expected asset.reimport to be published");
	Require(operations.Supports("asset.list"), "Expected asset.list to be published");
	Require(operations.Supports("asset.register_mesh"), "Expected asset.register_mesh to be published through the operation registry");
	Require(operations.Supports("validation.validate"), "Expected validation.validate to be published through the operation registry");
	Require(!operations.Supports("project.missing"), "Expected unsupported operations to stay absent from the registry");

	const auto* validationDescriptor = operations.FindOperation("validation.validate");
	Require(validationDescriptor != nullptr, "Expected validation.validate to resolve from the registry");
	Require(validationDescriptor->Domain == HE::OperationDomain::Validation, "Expected validation.validate to belong to the validation domain");

	const auto smokeRoot = std::filesystem::temp_directory_path() / "HuaEngineApplicationOperationsSmoke";
	std::error_code errorCode;
	std::filesystem::remove_all(smokeRoot, errorCode);

	HE::ProjectContext projectContext;
	auto initializeProject = operations.InitializeProject(smokeRoot / "SmokeProject", &projectContext, "SmokeProject");
	Require(initializeProject.Succeeded(), "Expected project.initialize to succeed through ApplicationOperations");
	Require(initializeProject.Operation == "project.initialize", "Expected project.initialize result to preserve the stable operation id");

	auto manifestInit = operations.InitializeAssetManifest(projectContext);
	Require(manifestInit.Succeeded(), "Expected manifest init operation to succeed");
	Require(manifestInit.Payload.find("manifest_path") != manifestInit.Payload.end(), "Expected manifest path payload");
	Require(!manifestInit.Payload.at("manifest_path").empty(), "Expected manifest path payload to be non-empty");
	HE::AssetImportReport emptyImportReport;
	auto initializeAssets = operations.InitializeProjectAssets(projectContext, &emptyImportReport);
	Require(initializeAssets.Succeeded(), "Expected asset.initialize to succeed through ApplicationOperations");
	Require(initializeAssets.Operation == "asset.initialize", "Expected stable asset.initialize operation id");
	Require(emptyImportReport.TotalFileAssets == 0, "Expected empty project asset initialization report");
	Require(std::filesystem::is_directory(projectContext.RootPath / "Library" / "Artifacts"), "Expected project Library creation");

	HE::Ref<HE::Scene> scene;
	auto createScene = operations.CreateScene("OperationsScene", scene);
	Require(createScene.Succeeded(), "Expected scene.create to succeed through ApplicationOperations");
	Require(static_cast<bool>(scene), "Expected scene.create to populate an in-memory scene");
	const auto scenePath = projectContext.GetAssetRootPath() / "Scenes" / "Operations.scene";
	Require(operations.SaveScene(*scene, scenePath).Succeeded(), "Expected scene fixture save");
	HE::AssetGuid sceneGuid;
	Require(operations.RegisterSceneAsset(projectContext, scenePath, &sceneGuid).Succeeded() && !sceneGuid.empty(), "Expected native scene asset registration");
	HE::AssetInspectionSnapshot sceneInspection;
	Require(operations.InspectAsset(sceneGuid, sceneInspection).Succeeded(), "Expected scene inspection snapshot");
	Require(sceneInspection.Asset.Kind == HE::AssetKind::Scene && sceneInspection.ImporterId == "scene.native", "Expected native scene inspection identity");
	Require(sceneInspection.Health.State == HE::AssetImportHealthState::Current && sceneInspection.ArtifactRelativePath.empty(), "Expected scene source health without an artifact");
	HE::AssetGuid repeatedSceneGuid;
	Require(operations.RegisterSceneAsset(projectContext, scenePath, &repeatedSceneGuid).Succeeded() && repeatedSceneGuid == sceneGuid, "Expected stable scene asset identity");

	uint32_t entityId = 0;
	auto createEntity = operations.CreateSceneEntity(*scene, "OperationsEntity", &entityId);
	Require(createEntity.Succeeded(), "Expected scene.entity.create to succeed through ApplicationOperations");
	Require(createEntity.Operation == "scene.entity.create", "Expected scene.entity.create result to preserve the stable operation id");

	auto addMesh = operations.AddSceneComponent(*scene, entityId, HE::SceneComponentKind::Mesh);
	Require(addMesh.Succeeded(), "Expected scene.component.add to add MeshComponent through ApplicationOperations");
	auto addMaterial = operations.AddSceneComponent(*scene, entityId, HE::SceneComponentKind::Material);
	Require(addMaterial.Succeeded(), "Expected scene.component.add to add MaterialComponent through ApplicationOperations");

	auto runtimeMesh = HE::Mesh::CreateQuad("OperationsQuad");
	Require(static_cast<bool>(runtimeMesh), "Expected runtime mesh creation to succeed");

	const auto importedMeshPath = projectContext.GetAssetRootPath() / "Meshes" / "ImportedQuad.mesh";
	std::filesystem::create_directories(importedMeshPath.parent_path(), errorCode);
	Require(!errorCode, "Expected imported mesh directory creation to succeed");
	Require(HE::Mesh::SaveToFile(*runtimeMesh, importedMeshPath.string()), "Expected imported mesh fixture to be saved");

	HE::AssetGuid importedGuid;
	auto importMesh = operations.ImportAsset(projectContext, "Meshes/ImportedQuad.mesh", HE::AssetKind::Mesh, &importedGuid);
	Require(importMesh.Succeeded(), "Expected asset.import to succeed through ApplicationOperations");
	Require(importMesh.Operation == "asset.import", "Expected asset.import result to preserve the stable operation id");
	Require(!importedGuid.empty(), "Expected asset.import to return an asset guid");
	const auto* importedMeshArtifact = application.Services().Assets().GetLibrary().Find(importedGuid);
	Require(importedMeshArtifact != nullptr, "Expected asset.import to commit the mesh artifact");
	Require(
		std::filesystem::is_regular_file(application.Services().Assets().GetLibrary().GetRootPath() / importedMeshArtifact->ArtifactRelativePath),
		"Expected asset.import mesh artifact file");

	const auto texturePath = projectContext.GetAssetRootPath() / "Textures" / "Imported.png";
	Require(HE::Tests::WriteTinyPng(texturePath), "Expected texture source fixture creation to succeed");

	HE::AssetGuid textureGuid;
	auto importTexture = operations.ImportAsset(projectContext, "Textures/Imported.png", HE::AssetKind::Texture2D, &textureGuid);
	Require(importTexture.Succeeded(), "Expected PNG asset.import to commit a runtime artifact");
	Require(importTexture.Operation == "asset.import", "Expected texture asset.import result to preserve the stable operation id");
	Require(!textureGuid.empty(), "Expected PNG import to return an asset guid");
	Require(application.Services().Assets().GetLibrary().Find(textureGuid) != nullptr, "Expected PNG asset.import artifact");
	Require(operations.CanImportAssetSource(texturePath), "Expected application import capability query");
	Require(!operations.CanImportAssetSource(texturePath.parent_path() / "Unsupported.txt"), "Expected unsupported application import capability query");
	HE::AssetReimportReport reimportReport;
	auto reimportTexture = operations.ReimportAssets(projectContext, texturePath, &reimportReport);
	Require(reimportTexture.Succeeded(), "Expected asset.reimport to succeed through ApplicationOperations");
	Require(reimportTexture.Operation == "asset.reimport", "Expected stable asset.reimport operation id");
	Require(reimportReport.ReimportedAssets == 1, "Expected application reimport report");

	HE::AssetHandle meshHandle = 0;
	const auto registeredMeshPath = projectContext.GetAssetRootPath() / "Meshes" / "OperationsQuad.mesh";
	std::filesystem::create_directories(registeredMeshPath.parent_path(), errorCode);
	Require(!errorCode, "Expected registered mesh directory creation to succeed");
	Require(HE::Mesh::SaveToFile(*runtimeMesh, registeredMeshPath.string()), "Expected registered mesh fixture to be saved");
	auto registerMesh = operations.RegisterMeshAsset(projectContext, "Meshes/OperationsQuad.mesh", runtimeMesh, &meshHandle);
	Require(registerMesh.Succeeded(), "Expected asset.register_mesh to succeed through ApplicationOperations");
	Require(meshHandle != 0, "Expected asset.register_mesh to assign a stable asset handle");
	Require(operations.InitializeProjectAssets(projectContext).Succeeded(), "Expected newly registered mesh artifact initialization");

	std::vector<HE::AssetRecord> records;
	auto listAssets = operations.ListAssets(projectContext, records);
	Require(listAssets.Succeeded(), "Expected asset.list to succeed through ApplicationOperations");
	Require(listAssets.Payload.find("asset_count") != listAssets.Payload.end(), "Expected asset.list asset_count payload");
	Require(!records.empty(), "Expected asset.list to return manifest registry records");
	HE::AssetRecord registeredMeshRecord;
	Require(operations.ResolveAsset(meshHandle, registeredMeshRecord).Succeeded(), "Expected registered mesh metadata resolve");
	HE::AssetInspectionSnapshot inspection;
	Require(operations.InspectAsset(registeredMeshRecord.Guid, inspection).Succeeded(), "Expected asset inspection snapshot");
	Require(inspection.Asset.Guid == registeredMeshRecord.Guid, "Expected inspection asset identity");
	Require(inspection.ImporterId == "hua.mesh-yaml", "Expected inspection importer identity");
	Require(inspection.MeshStatistics.has_value() && inspection.MeshStatistics->VertexCount > 0 && inspection.MeshStatistics->IndexCount > 0, "Expected mesh artifact statistics");
	Require(!inspection.ImportFingerprint.empty() && !inspection.ArtifactRelativePath.empty(), "Expected inspection artifact metadata");
	Require(inspection.Health.State == HE::AssetImportHealthState::Current, "Expected current inspection health");

	HE::ApplicationValidationRequest validationRequest;
	validationRequest.Project = &projectContext;
	validationRequest.SceneTarget = scene.get();
	validationRequest.IncludeAssets = true;

	auto validate = operations.Validate(validationRequest);
	Require(validate.Succeeded(), "Expected validation.validate to succeed through ApplicationOperations");
	Require(validate.Operation == "validation.validate", "Expected validation.validate result to preserve the stable operation id");

	auto removeMaterial = operations.RemoveSceneComponent(*scene, entityId, HE::SceneComponentKind::Material);
	Require(removeMaterial.Succeeded(), "Expected scene.component.remove to succeed through ApplicationOperations");

	const std::array<uint32_t, 1> deleteIds = { entityId };
	auto deleteEntity = operations.DeleteSceneEntities(*scene, deleteIds);
	Require(deleteEntity.Succeeded(), "Expected scene.entity.delete to succeed through ApplicationOperations");
	Require(deleteEntity.Operation == "scene.entity.delete", "Expected scene.entity.delete result to preserve the stable operation id");

	std::filesystem::remove_all(smokeRoot, errorCode);
	Require(!errorCode, "Expected ApplicationOperations smoke temporary directory cleanup to succeed");

	std::cout << "ApplicationOperationsSmoke passed" << std::endl;
	return 0;
}
