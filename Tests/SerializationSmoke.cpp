#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "HuaEngine/ECS/ComponentRegistry.h"
#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/Scene/Scene.h"
#include "HuaEngine/Scene/SceneSerializer.h"
#include "HuaEngine/Serialization/Serialization.h"

namespace HE {
	struct SmokePlayerComponent {
		TransformComponent Transform;
		std::string Name = "Player";
		int Level = 1;
		float Health = 100.0f;
		glm::vec3 SpawnPoint = { 0.0f, 0.0f, 0.0f };
		std::vector<std::string> Inventory;
	};
}

srefl_class(HE::SmokePlayerComponent,
	fields(
		field(Transform),
		field(Name),
		field(Level),
		field(Health),
		field(SpawnPoint),
		field(Inventory)
	)
)

namespace {
	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[SerializationSmoke] " << message << std::endl;
			std::exit(1);
		}
	}
}

int main() {
	HE::Log::Init({ .EnableConsoleOutput = false });
	HE::Serialization::InitializeSerialization();

	HE::SmokePlayerComponent player;
	player.Name = "Hero";
	player.Level = 10;
	player.Health = 85.5f;
	player.SpawnPoint = { 10.0f, 20.0f, 30.0f };
	player.Transform.Position = { 1.0f, 2.0f, 3.0f };
	player.Inventory = { "sword", "shield", "potion" };

	const std::string playerJson = HE::Serialization::ToJson(player);
	Require(playerJson.find("\"Name\"") != std::string::npos, "Expected reflected player serialization to use current field names");

	HE::SmokePlayerComponent loadedPlayer;
	Require(HE::Serialization::FromJson(playerJson, loadedPlayer), "Expected reflected player JSON to deserialize");
	Require(loadedPlayer.Name == "Hero", "Expected player name to round-trip");
	Require(loadedPlayer.Level == 10, "Expected player level to round-trip");
	Require(loadedPlayer.Inventory.size() == 3, "Expected player inventory to round-trip");
	Require(loadedPlayer.Transform.Position == glm::vec3(1.0f, 2.0f, 3.0f), "Expected nested transform to round-trip");

	HE::ComponentRegistry registry;
	HE::RegisterCoreComponents(registry);
	const HE::ComponentMetadata* transformMetadata = registry.FindByName("TransformComponent");
	Require(transformMetadata != nullptr, "Expected TransformComponent metadata to be registered");
	Require(transformMetadata->Serialize != nullptr, "Expected TransformComponent metadata serializer");
	Require(transformMetadata->Deserialize != nullptr, "Expected TransformComponent metadata deserializer");
	Require(transformMetadata->ConstructDefault != nullptr, "Expected TransformComponent metadata default constructor");
	Require(transformMetadata->Destroy != nullptr, "Expected TransformComponent metadata destroy function");
	Require(transformMetadata->Copy != nullptr, "Expected TransformComponent metadata copy function");
	Require(transformMetadata->AddCopyToWorld != nullptr, "Expected TransformComponent metadata world copy function");
	Require(transformMetadata->Size == sizeof(HE::TransformComponent), "Expected TransformComponent metadata size to match component size");

	HE::TransformComponent sourceTransform;
	sourceTransform.Position = { 1.0f, 2.0f, 3.0f };
	sourceTransform.Rotation = { 4.0f, 5.0f, 6.0f };
	sourceTransform.Scale = { 7.0f, 8.0f, 9.0f };

	HE::Serialization::JsonSerializationBackend transformWriteBackend;
	transformMetadata->Serialize(transformWriteBackend, transformMetadata->TypeName, &sourceTransform);
	const std::string transformMetadataJson = transformWriteBackend.SaveToString();
	Require(transformMetadataJson.find("\"Position\"") != std::string::npos, "Expected metadata serialization to emit Position");
	Require(transformMetadataJson.find("\"Rotation\"") != std::string::npos, "Expected metadata serialization to emit Rotation");
	Require(transformMetadataJson.find("\"Scale\"") != std::string::npos, "Expected metadata serialization to emit Scale");

	HE::TransformComponent loadedMetadataTransform;
	HE::Serialization::JsonSerializationBackend transformReadBackend;
	transformReadBackend.LoadFromString(transformMetadataJson);
	Require(transformMetadata->Deserialize(transformReadBackend, transformMetadata->TypeName, &loadedMetadataTransform), "Expected metadata transform JSON to deserialize");
	Require(loadedMetadataTransform.Position == sourceTransform.Position, "Expected metadata Position to round-trip");
	Require(loadedMetadataTransform.Rotation == sourceTransform.Rotation, "Expected metadata Rotation to round-trip");
	Require(loadedMetadataTransform.Scale == sourceTransform.Scale, "Expected metadata Scale to round-trip");

	std::vector<HE::TransformComponent> transforms(3);
	transforms[0].Position = { 1.0f, 0.0f, 0.0f };
	transforms[1].Position = { 0.0f, 1.0f, 0.0f };
	transforms[2].Position = { 0.0f, 0.0f, 1.0f };

	const std::string transformsJson = HE::Serialization::ToJson(transforms);
	std::vector<HE::TransformComponent> loadedTransforms;
	Require(HE::Serialization::FromJson(transformsJson, loadedTransforms), "Expected vector JSON to deserialize");
	Require(loadedTransforms.size() == 3, "Expected vector size to round-trip");
	Require(loadedTransforms[2].Position.z == 1.0f, "Expected vector element payload to round-trip");

	HE::Scene scene("SerializationSmokeScene");
	auto entity = scene.GetWorld().CreateEntity("Serialized Entity");
	entity.GetComponent<HE::TransformComponent>().Position = { 3.0f, 4.0f, 5.0f };

	const auto uuid = entity.GetUuid();
	const auto scenePath = std::filesystem::temp_directory_path() / "HuaEngineSerializationSmoke.scene";
	Require(HE::Serialization::SaveScene(scene, scenePath.string()), "Expected scene save to succeed");

	HE::Scene loadedScene;
	Require(HE::Serialization::LoadScene(scenePath.string(), loadedScene), "Expected scene load to succeed");
	auto loadedEntity = loadedScene.GetWorld().GetEntity(uuid);
	Require(loadedEntity.IsValid(), "Expected scene entity uuid to round-trip");
	Require(loadedEntity.GetName() == "Serialized Entity", "Expected scene entity name to round-trip");
	Require(loadedEntity.GetComponent<HE::TransformComponent>().Position == glm::vec3(3.0f, 4.0f, 5.0f), "Expected scene transform to round-trip");

	std::error_code errorCode;
	std::filesystem::remove(scenePath, errorCode);

	std::cout << "SerializationSmoke passed" << std::endl;
	return 0;
}
