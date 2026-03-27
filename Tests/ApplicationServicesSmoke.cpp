#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "HuaEngine.h"
#include "HuaEngine/Validation/ValidationService.h"
#include "HuaEngine/Application/ApplicationServices.h"
#include "HuaEngine/Script/ScriptRuntimeSystem.h"

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

	HE::AssetHandle meshHandle = 0;
	auto registerMesh = services.Assets().RegisterMeshAsset(projectContext, "Meshes/ServicesQuad.mesh", runtimeMesh, &meshHandle);
	Require(registerMesh.Succeeded(), "Expected ApplicationServices asset registration to succeed");
	Require(meshHandle != 0, "Expected ApplicationServices asset registration to assign a handle");

	HE::Scene scene("ApplicationServicesScene");
	scene.AddSyetem(HE::CreateRef<HE::ScriptRuntimeSystem>(scene, services.Scripts()));
	auto entity = scene.GetEntityManager().CreateEntity();
	entity.AddComponent<HE::MeshComponent>("ServicesQuad");
	entity.AddComponent<HE::MaterialComponent>();

	HE::ValidationRequest request;
	request.Project = &projectContext;
	request.SceneTarget = &scene;
	request.Assets = &services.Assets();
	request.ScriptScene = &scene;
	request.Scripts = &services.Scripts();

	HE::ValidationReport report;
	auto validationResult = services.Validation().Validate(request, &report);
	Require(validationResult.Succeeded(), "Expected ApplicationServices validation to succeed through the shared composition root");
	Require(report.DomainCount == 4, "Expected ApplicationServices validation to cover four domains");
	Require(report.AssetStatus.TotalAssets == 1, "Expected ApplicationServices validation to observe the asset registered through the same service root");

	std::filesystem::remove_all(smokeRoot, errorCode);
	Require(!errorCode, "Expected ApplicationServices smoke temporary directory cleanup to succeed");

	std::cout << "ApplicationServicesSmoke passed" << std::endl;
	return 0;
}
