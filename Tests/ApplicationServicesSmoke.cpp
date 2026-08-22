#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "HuaEngine.h"
#include "HuaEngine/Validation/ValidationService.h"
#include "HuaEngine/Application/ApplicationServices.h"

namespace {
	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[ApplicationServicesSmoke] " << message << std::endl;
			std::exit(1);
		}
	}
}

int main() {
	HE::Log::Init();
	HE::Serialization::InitializeSerialization();

	HE::ApplicationServices services;

	const auto smokeRoot = std::filesystem::temp_directory_path() / "HuaEngineApplicationServicesSmoke";
	std::error_code errorCode;
	std::filesystem::remove_all(smokeRoot, errorCode);

	HE::ProjectContext projectContext;
	auto initializeProject = services.Projects().InitializeProject(smokeRoot / "SmokeProject", &projectContext, "SmokeProject");
	Require(initializeProject.Succeeded(), "Expected ApplicationServices project root to initialize");

	auto runtimeMesh = HE::Mesh::CreateQuad("ServicesQuad");
	Require(static_cast<bool>(runtimeMesh), "Expected runtime mesh creation to succeed");
	const auto meshPath = projectContext.GetAssetRootPath() / "Meshes" / "ServicesQuad.mesh";
	std::filesystem::create_directories(meshPath.parent_path(), errorCode);
	Require(!errorCode, "Expected ApplicationServices mesh directory creation");
	Require(HE::Mesh::SaveToFile(*runtimeMesh, meshPath.generic_string()), "Expected ApplicationServices mesh source persistence");

	HE::AssetHandle meshHandle = 0;
	auto registerMesh = services.Assets().RegisterMeshAsset(projectContext, "Meshes/ServicesQuad.mesh", runtimeMesh, &meshHandle);
	Require(registerMesh.Succeeded(), "Expected ApplicationServices asset registration to succeed");
	Require(meshHandle != 0, "Expected ApplicationServices asset registration to assign a handle");
	HE::AssetRecord meshRecord;
	auto resolveMesh = services.Assets().ResolveAsset("Meshes/ServicesQuad.mesh", meshRecord);
	Require(resolveMesh.Succeeded(), "Expected ApplicationServices asset registration to expose an asset record");
	Require(!meshRecord.Guid.empty(), "Expected ApplicationServices mesh asset record to expose a stable guid");
	HE::MeshAssetRef meshReference;
	meshReference.Reference.Guid = meshRecord.Guid;

	HE::Scene scene("ApplicationServicesScene");
	auto entity = scene.GetWorld().CreateEntity();
	entity.AddComponent<HE::MeshComponent>(meshReference);
	entity.AddComponent<HE::MaterialComponent>();

	HE::ValidationRequest request;
	request.Project = &projectContext;
	request.SceneTarget = &scene;
	request.Assets = &services.Assets();

	HE::ValidationReport report;
	auto validationResult = services.Validation().Validate(request, &report);
	Require(validationResult.Succeeded(), "Expected ApplicationServices validation to succeed through the shared composition root");
	Require(report.DomainCount == 3, "Expected ApplicationServices validation to cover three domains");
	Require(
		report.AssetStatus.TotalAssets == services.Assets().GetAssetRegistry().GetAssetCount(),
		"Expected ApplicationServices validation to observe the shared asset registry");
	Require(report.AssetStatus.MeshAssets >= 1, "Expected ApplicationServices validation to include the registered mesh");

	std::filesystem::remove_all(smokeRoot, errorCode);
	Require(!errorCode, "Expected ApplicationServices smoke temporary directory cleanup to succeed");

	std::cout << "ApplicationServicesSmoke passed" << std::endl;
	return 0;
}
