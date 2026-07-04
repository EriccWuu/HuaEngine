#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/Scene/Scene.h"
#include "HuaEngine/Scene/SceneSerializer.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {
	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[ECSSceneSerializationSmoke] " << message << std::endl;
			std::exit(1);
		}
	}
}

int main() {
	HE::Serialization::InitializeSerialization();

	HE::Scene scene("Serialized ECS Scene");
	auto entity = scene.GetWorld().CreateEntity("Camera");
	auto& transform = entity.AddComponent<HE::TransformComponent>();
	transform.Position.x = 3.0f;
	transform.Position.y = 4.0f;
	transform.Rotation = { 10.0f, 20.0f, 30.0f };
	transform.Scale = { 2.0f, 3.0f, 4.0f };

	const auto uuid = entity.GetUuid();
	const std::filesystem::path path = "ecs_scene_serialization_smoke.scene";
	const bool saved = HE::Serialization::SaveScene(scene, path.string());
	Require(saved, "Expected scene save to succeed");

	std::ifstream savedFile(path);
	Require(savedFile.is_open(), "Expected saved scene file to be readable");
	const std::string savedText((std::istreambuf_iterator<char>(savedFile)), std::istreambuf_iterator<char>());
	savedFile.close();
	Require(savedText.find("\"TransformComponent\"") != std::string::npos, "Expected saved scene to include TransformComponent metadata name");
	Require(savedText.find("\"Position\"") != std::string::npos, "Expected saved scene to include reflected Position field");
	Require(savedText.find("\"Rotation\"") != std::string::npos, "Expected saved scene to include reflected Rotation field");
	Require(savedText.find("\"Scale\"") != std::string::npos, "Expected saved scene to include reflected Scale field");

	HE::Scene loaded;
	const bool loadedOk = HE::Serialization::LoadScene(path.string(), loaded);
	Require(loadedOk, "Expected scene load to succeed");
	Require(loaded.GetName() == "Serialized ECS Scene", "Expected scene name to round-trip");
	Require(loaded.GetWorld().GetEntityCount() == 1, "Expected one scene entity to round-trip");

	auto loadedEntity = loaded.GetWorld().GetEntity(uuid);
	Require(loadedEntity.IsValid(), "Expected scene entity uuid to round-trip");
	Require(loadedEntity.GetName() == "Camera", "Expected scene entity name to round-trip");
	Require(loadedEntity.HasComponent<HE::TransformComponent>(), "Expected scene entity transform to round-trip");
	const auto& loadedTransform = loadedEntity.GetComponent<HE::TransformComponent>();
	Require(loadedTransform.Position.x == 3.0f, "Expected Position.x to round-trip");
	Require(loadedTransform.Position.y == 4.0f, "Expected Position.y to round-trip");
	Require(loadedTransform.Rotation == glm::vec3(10.0f, 20.0f, 30.0f), "Expected Rotation to round-trip");
	Require(loadedTransform.Scale == glm::vec3(2.0f, 3.0f, 4.0f), "Expected Scale to round-trip");

	std::filesystem::remove(path);
	return 0;
}
