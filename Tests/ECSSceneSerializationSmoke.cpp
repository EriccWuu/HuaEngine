#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/Asset/AssetTypes.h"
#include "HuaEngine/Scene/Scene.h"
#include "HuaEngine/Scene/SceneSerializer.h"
#include "Module/Rendering/RenderingComponent.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <variant>

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
	std::error_code removeError;

	HE::Scene scene("Serialized ECS Scene");
	auto entity = scene.GetWorld().CreateEntity("Camera");
	auto& transform = entity.AddComponent<HE::TransformComponent>();
	transform.Position.x = 3.0f;
	transform.Position.y = 4.0f;
	transform.Rotation = { 10.0f, 20.0f, 30.0f };
	transform.Scale = { 2.0f, 3.0f, 4.0f };
	auto& mesh = entity.AddComponent<HE::Rendering::MeshComponent>();
	mesh.Mesh.Reference.Guid = HE::BuiltinAssetGuids::QuadMesh;
	auto& material = entity.AddComponent<HE::Rendering::MaterialComponent>();
	material.Material.Reference.Guid = HE::BuiltinAssetGuids::DefaultMaterial;
	material.Overrides.SetVec4("u_BaseColor", glm::vec4(1.0f, 0.0f, 1.0f, 1.0f));

	const auto uuid = entity.GetUuid();
	const std::filesystem::path path = std::filesystem::temp_directory_path() / "HuaEngineECSSceneSerializationSmoke.scene";
	const bool saved = HE::Serialization::SaveScene(scene, path.string());
	Require(saved, "Expected scene save to succeed");

	std::ifstream savedFile(path);
	Require(savedFile.is_open(), "Expected saved scene file to be readable");
	const std::string savedText((std::istreambuf_iterator<char>(savedFile)), std::istreambuf_iterator<char>());
	savedFile.close();
	Require(savedText.find("TransformComponent:") != std::string::npos, "Expected saved scene to include TransformComponent metadata name");
	Require(savedText.find("Position:") != std::string::npos, "Expected saved scene to include reflected Position field");
	Require(savedText.find("Rotation:") != std::string::npos, "Expected saved scene to include reflected Rotation field");
	Require(savedText.find("Scale:") != std::string::npos, "Expected saved scene to include reflected Scale field");
	Require(savedText.find("\"TransformComponent\"") == std::string::npos, "Expected saved scene to use YAML key style instead of JSON object syntax");
	Require(savedText.find("builtin-mesh-quad") != std::string::npos, "Expected mesh GUID in scene");
	Require(savedText.find("builtin-material-default") != std::string::npos, "Expected material GUID in scene");
	Require(savedText.find("u_BaseColor") != std::string::npos, "Expected material override parameter in scene");

	HE::Scene loaded;
	const bool loadedOk = HE::Serialization::LoadScene(path.string(), loaded);
	Require(loadedOk, "Expected scene load to succeed");
	Require(loaded.GetName() == "Serialized ECS Scene", "Expected scene name to round-trip");
	Require(loaded.GetWorld().GetEntityCount() == 1, "Expected one scene entity to round-trip");

	auto loadedEntity = loaded.GetWorld().GetEntity(uuid);
	Require(loadedEntity.IsValid(), "Expected scene entity uuid to round-trip");
	Require(loadedEntity.GetName() == "Camera", "Expected scene entity name to round-trip");
	Require(loadedEntity.HasComponent<HE::TransformComponent>(), "Expected scene entity transform to round-trip");
	Require(loadedEntity.HasComponent<HE::Rendering::MeshComponent>(), "Expected scene entity mesh to round-trip");
	Require(loadedEntity.HasComponent<HE::Rendering::MaterialComponent>(), "Expected scene entity material to round-trip");
	const auto& loadedTransform = loadedEntity.GetComponent<HE::TransformComponent>();
	Require(loadedTransform.Position.x == 3.0f, "Expected Position.x to round-trip");
	Require(loadedTransform.Position.y == 4.0f, "Expected Position.y to round-trip");
	Require(loadedTransform.Rotation == glm::vec3(10.0f, 20.0f, 30.0f), "Expected Rotation to round-trip");
	Require(loadedTransform.Scale == glm::vec3(2.0f, 3.0f, 4.0f), "Expected Scale to round-trip");
	const auto& loadedMesh = loadedEntity.GetComponent<HE::Rendering::MeshComponent>();
	Require(loadedMesh.Mesh.Reference.Guid == HE::BuiltinAssetGuids::QuadMesh, "Expected Mesh GUID to round-trip");
	const auto& loadedMaterial = loadedEntity.GetComponent<HE::Rendering::MaterialComponent>();
	Require(loadedMaterial.Material.Reference.Guid == HE::BuiltinAssetGuids::DefaultMaterial, "Expected Material GUID to round-trip");
	const auto overrideIt = loadedMaterial.Overrides.Parameters.find("u_BaseColor");
	Require(overrideIt != loadedMaterial.Overrides.Parameters.end(), "Expected vec4 material override to round-trip");
	Require(std::holds_alternative<glm::vec4>(overrideIt->second), "Expected u_BaseColor override to be vec4");
	Require(std::get<glm::vec4>(overrideIt->second) == glm::vec4(1.0f, 0.0f, 1.0f, 1.0f), "Expected vec4 override value to round-trip");

	std::filesystem::remove(path, removeError);
	return 0;
}
