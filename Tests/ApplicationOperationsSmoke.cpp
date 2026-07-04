#include <cstdlib>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "HuaEngine.h"

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
	Require(operations.Supports("asset.import"), "Expected asset.import to be published");
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

	HE::Ref<HE::Scene> scene;
	auto createScene = operations.CreateScene("OperationsScene", scene);
	Require(createScene.Succeeded(), "Expected scene.create to succeed through ApplicationOperations");
	Require(static_cast<bool>(scene), "Expected scene.create to populate an in-memory scene");

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

	const auto texturePath = projectContext.GetAssetRootPath() / "Textures" / "SourceOnly.texture2d";
	std::filesystem::create_directories(texturePath.parent_path(), errorCode);
	Require(!errorCode, "Expected texture source directory creation to succeed");
	{
		std::ofstream textureStream(texturePath, std::ios::out | std::ios::binary);
		textureStream << "source-only texture placeholder";
	}

	HE::AssetGuid textureGuid;
	auto importTexture = operations.ImportAsset(projectContext, "Textures/SourceOnly.texture2d", HE::AssetKind::Texture2D, &textureGuid);
	Require(importTexture.RequiresManualIntervention(), "Expected texture asset.import to require manual intervention while loader is unsupported");
	Require(importTexture.Operation == "asset.import", "Expected texture asset.import result to preserve the stable operation id");
	Require(!textureGuid.empty(), "Expected source-only texture import to still return an asset guid");
	Require(importTexture.Payload.find("source_only") != importTexture.Payload.end(), "Expected texture import to report source_only payload");

	HE::AssetHandle meshHandle = 0;
	const auto registeredMeshPath = projectContext.GetAssetRootPath() / "Meshes" / "OperationsQuad.mesh";
	std::filesystem::create_directories(registeredMeshPath.parent_path(), errorCode);
	Require(!errorCode, "Expected registered mesh directory creation to succeed");
	Require(HE::Mesh::SaveToFile(*runtimeMesh, registeredMeshPath.string()), "Expected registered mesh fixture to be saved");
	auto registerMesh = operations.RegisterMeshAsset(projectContext, "Meshes/OperationsQuad.mesh", runtimeMesh, &meshHandle);
	Require(registerMesh.Succeeded(), "Expected asset.register_mesh to succeed through ApplicationOperations");
	Require(meshHandle != 0, "Expected asset.register_mesh to assign a stable asset handle");

	std::vector<HE::AssetRecord> records;
	auto listAssets = operations.ListAssets(projectContext, records);
	Require(listAssets.Succeeded(), "Expected asset.list to succeed through ApplicationOperations");
	Require(listAssets.Payload.find("asset_count") != listAssets.Payload.end(), "Expected asset.list asset_count payload");
	Require(!records.empty(), "Expected asset.list to return manifest registry records");

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
