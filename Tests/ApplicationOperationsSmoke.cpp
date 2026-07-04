#include <cstdlib>
#include <array>
#include <filesystem>
#include <iostream>
#include <string>

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

	HE::AssetHandle meshHandle = 0;
	auto registerMesh = operations.RegisterMeshAsset(projectContext, "Meshes/OperationsQuad.mesh", runtimeMesh, &meshHandle);
	Require(registerMesh.Succeeded(), "Expected asset.register_mesh to succeed through ApplicationOperations");
	Require(meshHandle != 0, "Expected asset.register_mesh to assign a stable asset handle");

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
