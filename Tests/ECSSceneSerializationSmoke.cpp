#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/Asset/AssetManifest.h"
#include "HuaEngine/Asset/AssetTypes.h"
#include "HuaEngine/Project/ProjectService.h"
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
	Require(savedText.find("\"TransformComponent\"") != std::string::npos, "Expected saved scene to include TransformComponent metadata name");
	Require(savedText.find("\"Position\"") != std::string::npos, "Expected saved scene to include reflected Position field");
	Require(savedText.find("\"Rotation\"") != std::string::npos, "Expected saved scene to include reflected Rotation field");
	Require(savedText.find("\"Scale\"") != std::string::npos, "Expected saved scene to include reflected Scale field");
	Require(savedText.find("MeshAssetName") == std::string::npos, "Expected new scene format to omit MeshAssetName");
	Require(savedText.find("MaterialInstance") == std::string::npos, "Expected new scene format to omit MaterialInstance");
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

	const std::filesystem::path legacyPath = std::filesystem::temp_directory_path() / "HuaEngineECSSceneSerializationSmokeLegacy.scene";
	std::ofstream legacyFile(legacyPath, std::ios::trunc);
	Require(legacyFile.is_open(), "Expected legacy scene file to be writable");
	legacyFile << R"({
  "entities": [
    {
      "components": {
        "MeshComponent": {
          "MeshAssetName": "Cube"
        },
        "MaterialComponent": {
          "MaterialInstance": {
            "base_material": {
              "name": "LegacyMaterial",
              "type": "Standard",
              "parameters": {},
              "texture_slots": {}
            },
            "parameter_overrides": {
              "u_BaseColor": {
                "value_type": "Vec4",
                "value": {
                  "x": 0.25,
                  "y": 0.5,
                  "z": 0.75,
                  "w": 1.0
                }
              }
            }
          },
          "BlendMode": "Transparent"
        }
      },
      "name": "Legacy Entity",
      "uuid": "00000000000000000000000000000088"
    }
  ],
  "name": "Legacy Scene",
  "version": 3
})";
	legacyFile.close();

	HE::Scene legacyLoaded;
	Require(HE::Serialization::LoadScene(legacyPath.string(), legacyLoaded), "Expected legacy scene load to migrate");
	auto legacyEntity = legacyLoaded.GetWorld().GetEntity(HE::EntityUuid::FromString("00000000000000000000000000000088"));
	Require(legacyEntity.IsValid(), "Expected legacy scene entity to load");
	Require(legacyEntity.HasComponent<HE::Rendering::MeshComponent>(), "Expected migrated mesh component");
	Require(legacyEntity.HasComponent<HE::Rendering::MaterialComponent>(), "Expected migrated material component");
	const auto& legacyMesh = legacyEntity.GetComponent<HE::Rendering::MeshComponent>();
	Require(legacyMesh.Mesh.Reference.Guid == HE::BuiltinAssetGuids::CubeMesh, "Expected legacy Cube mesh to migrate to builtin GUID");
	const auto& legacyMaterial = legacyEntity.GetComponent<HE::Rendering::MaterialComponent>();
	Require(legacyMaterial.Material.Reference.Guid == HE::BuiltinAssetGuids::DefaultMaterial, "Expected legacy MaterialInstance to migrate to default material GUID");
	Require(legacyMaterial.BlendMode == HE::Rendering::MaterialBlendMode::Transparent, "Expected legacy blend mode to load");
	const auto legacyOverrideIt = legacyMaterial.Overrides.Parameters.find("u_BaseColor");
	Require(legacyOverrideIt != legacyMaterial.Overrides.Parameters.end(), "Expected legacy vec4 override to migrate");
	Require(std::get<glm::vec4>(legacyOverrideIt->second) == glm::vec4(0.25f, 0.5f, 0.75f, 1.0f), "Expected legacy vec4 override value to migrate");

	const std::filesystem::path migratedPath = std::filesystem::temp_directory_path() / "HuaEngineECSSceneSerializationSmokeMigrated.scene";
	Require(HE::Serialization::SaveScene(legacyLoaded, migratedPath.string()), "Expected migrated scene save to succeed");
	const std::string migratedText = [&]() {
		std::ifstream migratedFile(migratedPath);
		Require(migratedFile.is_open(), "Expected migrated scene file to be readable");
		return std::string((std::istreambuf_iterator<char>(migratedFile)), std::istreambuf_iterator<char>());
	}();
	Require(migratedText.find("MeshAssetName") == std::string::npos, "Expected migrated save to omit MeshAssetName");
	Require(migratedText.find("MaterialInstance") == std::string::npos, "Expected migrated save to omit MaterialInstance");
	Require(migratedText.find("builtin-mesh-cube") != std::string::npos, "Expected migrated save to contain cube GUID");

	const auto projectRoot = std::filesystem::temp_directory_path() / "HuaEngineECSSceneSerializationSmokeProject";
	std::filesystem::remove_all(projectRoot, removeError);
	HE::ProjectService projectService;
	HE::ProjectContext projectContext;
	Require(projectService.InitializeProject(projectRoot, &projectContext, "SceneMigrationProject").Succeeded(), "Expected project init for scene migration");
	HE::AssetManifest manifest;
	Require(HE::LoadOrCreateAssetManifest(projectContext, manifest).Succeeded(), "Expected project manifest init for scene migration");
	HE::AssetManifestRecord customMeshRecord;
	customMeshRecord.Guid = "custom-mesh-guid-smokequad";
	customMeshRecord.AssetId = "Meshes/SmokeQuad.mesh";
	customMeshRecord.Kind = HE::AssetKind::Mesh;
	customMeshRecord.Source = HE::AssetSource::File;
	customMeshRecord.RelativePath = "Meshes/SmokeQuad.mesh";
	customMeshRecord.ImportState = HE::AssetImportState::Registered;
	Require(manifest.Upsert(customMeshRecord), "Expected custom mesh manifest upsert");
	Require(HE::SaveAssetManifest(projectContext, manifest).Succeeded(), "Expected custom mesh manifest save");

	const auto legacyCustomPath = projectContext.GetSceneRootPath() / "LegacyCustomMesh.scene";
	std::ofstream legacyCustomFile(legacyCustomPath, std::ios::trunc);
	Require(legacyCustomFile.is_open(), "Expected custom legacy scene file to be writable");
	legacyCustomFile << R"({
  "entities": [
    {
      "components": {
        "MeshComponent": {
          "MeshAssetName": "Meshes/SmokeQuad.mesh"
        }
      },
      "name": "Legacy Custom Mesh",
      "uuid": "00000000000000000000000000000089"
    }
  ],
  "name": "Legacy Custom Mesh Scene",
  "version": 3
})";
	legacyCustomFile.close();

	HE::Scene legacyCustomLoaded;
	Require(HE::Serialization::LoadScene(legacyCustomPath.string(), legacyCustomLoaded), "Expected custom legacy mesh scene to load");
	auto legacyCustomEntity = legacyCustomLoaded.GetWorld().GetEntity(HE::EntityUuid::FromString("00000000000000000000000000000089"));
	Require(legacyCustomEntity.IsValid(), "Expected custom legacy mesh entity to load");
	Require(legacyCustomEntity.HasComponent<HE::Rendering::MeshComponent>(), "Expected custom legacy mesh component");
	const auto& legacyCustomMesh = legacyCustomEntity.GetComponent<HE::Rendering::MeshComponent>();
	Require(legacyCustomMesh.Mesh.Reference.Guid == customMeshRecord.Guid, "Expected legacy custom mesh asset id to migrate through manifest GUID lookup");

	const auto migratedCustomPath = projectContext.GetSceneRootPath() / "MigratedCustomMesh.scene";
	Require(HE::Serialization::SaveScene(legacyCustomLoaded, migratedCustomPath.string()), "Expected migrated custom mesh scene save");
	const std::string migratedCustomText = [&]() {
		std::ifstream migratedCustomFile(migratedCustomPath);
		Require(migratedCustomFile.is_open(), "Expected migrated custom scene file to be readable");
		return std::string((std::istreambuf_iterator<char>(migratedCustomFile)), std::istreambuf_iterator<char>());
	}();
	Require(migratedCustomText.find("MeshAssetName") == std::string::npos, "Expected custom migrated save to omit MeshAssetName");
	Require(migratedCustomText.find(customMeshRecord.Guid) != std::string::npos, "Expected custom migrated save to preserve manifest GUID");

	std::filesystem::remove(path, removeError);
	std::filesystem::remove(legacyPath, removeError);
	std::filesystem::remove(migratedPath, removeError);
	std::filesystem::remove_all(projectRoot, removeError);
	return 0;
}
