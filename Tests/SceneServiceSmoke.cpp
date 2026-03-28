#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "HuaEngine.h"
#include "HuaEngine/Scene/SceneService.h"

namespace {
	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[SceneServiceSmoke] " << message << std::endl;
			std::exit(1);
		}
	}

	std::string ReadFileText(const std::filesystem::path& path) {
		std::ifstream stream(path, std::ios::in | std::ios::binary);
		Require(stream.good(), "Expected file read to succeed: " + path.generic_string());
		return std::string((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
	}

	uint32_t CountEntities(HE::Scene& scene) {
		uint32_t entityCount = 0;
		auto& registry = scene.GetEntityManager().GetRegistry();
		for (auto [entity] : registry.storage<entt::entity>().each()) {
			if (!registry.valid(entity)) {
				continue;
			}

			(void)entity;
			++entityCount;
		}
		return entityCount;
	}

	entt::entity FindFirstLiveEntity(entt::registry& registry) {
		for (auto [entity] : registry.storage<entt::entity>().each()) {
			if (registry.valid(entity)) {
				return entity;
			}
		}

		return entt::null;
	}
}

int main() {
	HE::Log::Init();
	HE::Serialization::InitializeSerialization();

	const auto smokeRoot = std::filesystem::temp_directory_path() / "HuaEngineSceneServiceSmoke";
	std::error_code errorCode;
	std::filesystem::remove_all(smokeRoot, errorCode);
	std::filesystem::create_directories(smokeRoot, errorCode);
	Require(!errorCode, "Expected smoke root directory creation to succeed");

	HE::SceneService sceneService;
	HE::Ref<HE::Scene> scene;
	auto createResult = sceneService.CreateScene("SmokeScene", scene);
	Require(createResult.Succeeded(), "Expected scene.create to succeed");
	Require(static_cast<bool>(scene), "Expected created scene reference to be valid");
	Require(scene->GetName() == "SmokeScene", "Expected created scene name to match");

	auto firstEntity = scene->GetEntityManager().CreateEntity();
	firstEntity.GetComponent<HE::TransformComponent>().Position = { 1.0f, 2.0f, 3.0f };
	auto secondEntity = scene->GetEntityManager().CreateEntity();
	secondEntity.GetComponent<HE::TransformComponent>().Position = { -2.0f, 0.5f, 4.0f };
	auto renderEntity = scene->GetEntityManager().CreateEntity();
	renderEntity.AddComponent<HE::MeshComponent>("SmokeMesh");
	renderEntity.AddComponent<HE::MaterialComponent>();

	HE::SceneValidationReport validReport;
	auto validResult = sceneService.ValidateScene(*scene, &validReport);
	Require(validResult.Succeeded(), "Expected scene.validate to accept a valid smoke scene");
	Require(validReport.EntityCount == 3, "Expected valid smoke scene to contain three entities");
	Require(validReport.RenderEntitiesMissingMaterial == 0, "Expected valid smoke scene to keep formal render entities structurally complete");
	Require(validReport.EntitiesUsingLegacyRenderer == 0, "Expected valid smoke scene to avoid legacy renderer usage");

	const auto sceneFilePath = smokeRoot / "SmokeScene.scene";
	auto saveResult = sceneService.SaveScene(*scene, sceneFilePath);
	Require(saveResult.Succeeded(), "Expected scene.save to succeed");
	Require(std::filesystem::exists(sceneFilePath), "Expected saved scene file to exist");
	const auto sceneFileText = ReadFileText(sceneFilePath);
	Require(sceneFileText.find("\"components\": {") != std::string::npos, "Expected scene serialization to persist components as an object map");
	Require(sceneFileText.find("\"component_type_id\"") == std::string::npos, "Expected scene serialization to avoid legacy component_type_id fields");
	Require(sceneFileText.find("\"compId\"") == std::string::npos, "Expected scene serialization to avoid legacy compId fields");

	scene->GetEntityManager().DestroyEntity(secondEntity);
	auto saveAfterDelete = sceneService.SaveScene(*scene, sceneFilePath);
	Require(saveAfterDelete.Succeeded(), "Expected scene.save to succeed after deleting an entity");
	const auto sceneFileTextAfterDelete = ReadFileText(sceneFilePath);
	Require(sceneFileTextAfterDelete.find("\"components\": {}") == std::string::npos, "Expected deleted entities to be fully removed instead of persisting empty component shells");
	Require(sceneFileTextAfterDelete.find("\"id\": 1048576") == std::string::npos, "Expected tombstone entity identifiers to be excluded from scene serialization");
	Require(sceneFileTextAfterDelete.find("\"id\": 1048581") == std::string::npos, "Expected deleted entity tombstones to be excluded from scene serialization");

	HE::Ref<HE::Scene> loadedScene;
	auto loadResult = sceneService.LoadScene(sceneFilePath, loadedScene);
	Require(loadResult.Succeeded(), "Expected scene.load to succeed");
	Require(static_cast<bool>(loadedScene), "Expected loaded scene reference to be valid");
	Require(loadedScene->GetName() == "SmokeScene", "Expected loaded scene name to round-trip");
	Require(CountEntities(*loadedScene) == 2, "Expected loaded scene to contain two live entities after deletion save");

	HE::SceneValidationReport loadedReport;
	auto loadedValidation = sceneService.ValidateScene(*loadedScene, &loadedReport);
	Require(loadedValidation.Succeeded(), "Expected loaded scene to validate successfully");
	Require(loadedReport.RenderEntitiesMissingMaterial == 0, "Expected loaded scene render pair to remain valid after round-trip");
	Require(loadedReport.EntitiesUsingLegacyRenderer == 0, "Expected loaded scene to remain free of legacy renderers");

	auto& loadedRegistry = loadedScene->GetEntityManager().GetRegistry();
	const auto firstLoadedEntity = FindFirstLiveEntity(loadedRegistry);
	Require(firstLoadedEntity != entt::null, "Expected loaded scene to expose at least one live entity");
	loadedRegistry.emplace<HE::RendererComponent>(firstLoadedEntity);

	HE::SceneValidationReport legacyRendererReport;
	auto legacyRendererValidation = sceneService.ValidateScene(*loadedScene, &legacyRendererReport);
	Require(legacyRendererValidation.RequiresManualIntervention(), "Expected legacy RendererComponent usage to require manual intervention");
	Require(legacyRendererReport.EntitiesUsingLegacyRenderer == 1, "Expected degraded scene report to count exactly one entity using legacy renderer");

	loadedRegistry.remove<HE::RendererComponent>(firstLoadedEntity);
	loadedRegistry.remove<HE::TransformComponent>(firstLoadedEntity);

	HE::SceneValidationReport degradedReport;
	auto degradedValidation = sceneService.ValidateScene(*loadedScene, &degradedReport);
	Require(degradedValidation.RequiresManualIntervention(), "Expected missing TransformComponent to require manual intervention");
	Require(degradedReport.EntitiesMissingTransform == 1, "Expected degraded scene report to count exactly one entity missing TransformComponent");

	std::filesystem::remove_all(smokeRoot, errorCode);
	Require(!errorCode, "Expected smoke root cleanup to succeed");

	std::cout << "SceneServiceSmoke passed" << std::endl;
	return 0;
}
