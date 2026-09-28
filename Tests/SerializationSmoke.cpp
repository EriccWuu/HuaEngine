#include "HuaEngine/Generated/GeneratedReflection.h"
#include "ECSTestSupport.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include "HuaEngine/ECS/Runtime/TypeRegistry.h"
#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/Scene/Scene.h"
#include "HuaEngine/Scene/SceneSerializer.h"
#include "HuaEngine/Serialization/Serialization.h"
#include "HuaEngine/Serialization/YamlSerializationBackend.h"

namespace HE {
	struct SmokePlayerComponent {
		TransformComponent Transform;
		std::string Name = "Player";
		int Level = 1;
		float Health = 100.0f;
		glm::vec3 SpawnPoint = { 0.0f, 0.0f, 0.0f };
		std::vector<std::string> Inventory;
	};

	// This fixture deliberately has no component base or static reflection macros.
	struct PlainRuntimeValue {
		int32_t Count = 7;
		std::string Label = "unchanged";
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

	template<typename Action>
	void RequireLogicError(Action&& action, const std::string& message) {
		try {
			action();
		}
		catch (const std::logic_error&) {
			return;
		}
		Require(false, message);
	}

	void VerifyPlainRuntimeSerialization() {
		using HE::PlainRuntimeValue;
		static_assert(std::is_aggregate_v<PlainRuntimeValue>);
		static_assert(!HE::Refl::has_fields_v<HE::Refl::type_info<PlainRuntimeValue>>);
		static const HE::Refl::RuntimeFieldDescriptor runtimeFields[] = {
			{ .Name = "Count", .Type = "int32_t", .Offset = offsetof(PlainRuntimeValue, Count), .Size = sizeof(int32_t),
				.Flags = HE::Refl::RuntimeFieldFlags::Serializable,
				.Serialize = [](HE::Serialization::SerializationBackend& backend, const std::string& name, const void* value) {
					HE::Serialization::SerializeValue(backend, name, static_cast<const PlainRuntimeValue*>(value)->Count);
				},
				.Deserialize = [](HE::Serialization::SerializationBackend& backend, const std::string& name, void* value) {
					return HE::Serialization::DeserializeValue(backend, name, static_cast<PlainRuntimeValue*>(value)->Count);
				} },
			{ .Name = "Label", .Type = "std::string", .Offset = offsetof(PlainRuntimeValue, Label), .Size = sizeof(std::string),
				.Flags = HE::Refl::RuntimeFieldFlags::Serializable,
				.Serialize = [](HE::Serialization::SerializationBackend& backend, const std::string& name, const void* value) {
					HE::Serialization::SerializeValue(backend, name, static_cast<const PlainRuntimeValue*>(value)->Label);
				},
				.Deserialize = [](HE::Serialization::SerializationBackend& backend, const std::string& name, void* value) {
					return HE::Serialization::DeserializeValue(backend, name, static_cast<PlainRuntimeValue*>(value)->Label);
				} }
		};
		static const HE::Refl::RuntimeTypeDescriptor reflected{
			.Name = "PlainRuntimeValue", .QualifiedName = "HE::PlainRuntimeValue", .Kind = "component",
			.Size = sizeof(PlainRuntimeValue), .Fields = runtimeFields
		};
		HE::Ecs::EcsContext context;
		auto descriptor = HE::Ecs::MakeTypeDescriptor<PlainRuntimeValue>(
			HE::Ecs::TypeGuid{ 0xbdd824a5693e4511ULL, 0xbbcab9a274cfb8f3ULL }, "PlainRuntimeValue");
		descriptor.Reflection = &reflected;
		Require(context.Types().Register(descriptor).HasValue(), "Expected ordinary runtime-reflected struct registration");
		const PlainRuntimeValue source{ 42, "runtime-only value" };
		const std::string json = HE::Serialization::ToJson(source, context.Types());
		const std::string yaml = HE::Serialization::ToYaml(source, context.Types());
		Require(json.find("runtime-only value") != std::string::npos && yaml.find("runtime-only value") != std::string::npos,
			"Expected a plain runtime descriptor to serialize fields rather than an empty object");
		PlainRuntimeValue jsonValue;
		PlainRuntimeValue yamlValue;
		Require(HE::Serialization::FromJson(json, jsonValue, context.Types()) &&
			HE::Serialization::FromYaml(yaml, yamlValue, context.Types()), "Expected plain runtime JSON and YAML round-trip");
		Require(jsonValue.Count == 42 && yamlValue.Count == 42 && jsonValue.Label == source.Label && yamlValue.Label == source.Label,
			"Expected ordinary non-POD field values to round-trip through the owning Context");

		HE::Ecs::EcsContext wrongContext;
		Require(wrongContext.Types().Register<int32_t>("Fixture.Unrelated").HasValue(), "Expected an unrelated type in the wrong Context");
		HE::Ecs::EcsContext withoutReflection;
		descriptor.Reflection = nullptr;
		Require(withoutReflection.Types().Register(descriptor).HasValue(), "Expected registration alone to remain distinct from serialization capability");
		const HE::Ecs::TypeRegistry* unavailableTypes[] = { nullptr, &wrongContext.Types(), &withoutReflection.Types() };
		for (const auto* types : unavailableTypes) {
			RequireLogicError([&] {
				(void)(types ? HE::Serialization::ToJson(source, *types) : HE::Serialization::ToJson(source));
			}, "Expected missing runtime metadata to reject JSON output explicitly");
			RequireLogicError([&] {
				(void)(types ? HE::Serialization::ToYaml(source, *types) : HE::Serialization::ToYaml(source));
			}, "Expected missing runtime metadata to reject YAML output explicitly");
			PlainRuntimeValue unchanged;
			Require(!(types ? HE::Serialization::FromJson(json, unchanged, *types) : HE::Serialization::FromJson(json, unchanged)),
				"Expected missing runtime metadata to reject JSON input");
			Require(unchanged.Count == 7 && unchanged.Label == "unchanged", "Expected rejected JSON to preserve the destination");
			Require(!(types ? HE::Serialization::FromYaml(yaml, unchanged, *types) : HE::Serialization::FromYaml(yaml, unchanged)),
				"Expected missing runtime metadata to reject YAML input");
			Require(unchanged.Count == 7 && unchanged.Label == "unchanged", "Expected rejected YAML to preserve the destination");
		}
		const auto failedJson = (std::filesystem::temp_directory_path() / "plain-rejected.json").string();
		const auto failedYaml = (std::filesystem::temp_directory_path() / "plain-rejected.yaml").string();
		Require(!HE::Serialization::SaveAsJson(source, failedJson) && !HE::Serialization::SaveAsYaml(source, failedYaml, wrongContext.Types()),
			"Expected file helpers to return false for missing reflection instead of writing an empty object");
		Require(!std::filesystem::exists(failedJson) && !std::filesystem::exists(failedYaml), "Expected rejected serialization not to create files");
		std::cout << "Plain runtime struct serialization rejects missing or wrong Contexts" << std::endl;
	}
}

int main() {
	HE::Log::Init({ .EnableConsoleOutput = false });
	HE::Serialization::InitializeSerialization();
	VerifyPlainRuntimeSerialization();
	HE::Ecs::TypeRegistry registry;
	Require(HE::Generated::RegisterGeneratedComponents(registry).HasValue(), "Expected canonical module registration");

	auto yamlBackend = HE::Serialization::SerializationManager::Instance().CreateBackend(HE::Serialization::SerializationFormat::YAML);
	Require(yamlBackend != nullptr, "Expected YAML backend to be registered with serialization manager");

	HE::SmokePlayerComponent player;
	player.Name = "Hero";
	player.Level = 10;
	player.Health = 85.5f;
	player.SpawnPoint = { 10.0f, 20.0f, 30.0f };
	player.Transform.Position = { 1.0f, 2.0f, 3.0f };
	player.Inventory = { "sword", "shield", "potion" };

	const std::string playerJson = HE::Serialization::ToJson(player, registry);
	Require(playerJson.find("\"Name\"") != std::string::npos, "Expected reflected player serialization to use current field names");

	HE::SmokePlayerComponent loadedPlayer;
	Require(HE::Serialization::FromJson(playerJson, loadedPlayer, registry), "Expected reflected player JSON to deserialize");
	Require(loadedPlayer.Name == "Hero", "Expected player name to round-trip");
	Require(loadedPlayer.Level == 10, "Expected player level to round-trip");
	Require(loadedPlayer.Inventory.size() == 3, "Expected player inventory to round-trip");
	Require(loadedPlayer.Transform.Position == glm::vec3(1.0f, 2.0f, 3.0f), "Expected nested transform to round-trip");

	const std::string bomPlayerJson = std::string("\xEF\xBB\xBF") + playerJson;
	HE::SmokePlayerComponent loadedBomPlayer;
	Require(HE::Serialization::FromJson(bomPlayerJson, loadedBomPlayer, registry), "Expected JSON with UTF-8 BOM to deserialize");
	Require(loadedBomPlayer.Name == "Hero", "Expected BOM JSON player name to round-trip");

	const std::string playerYamlFromHelper = HE::Serialization::ToYaml(player, registry);
	Require(playerYamlFromHelper.find("Name: Hero") != std::string::npos, "Expected reflected player YAML helper to emit YAML mapping style");
	Require(playerYamlFromHelper.find("\"Name\"") == std::string::npos, "Expected reflected player YAML helper not to emit JSON object syntax");
	HE::SmokePlayerComponent loadedYamlPlayer;
	Require(HE::Serialization::FromYaml(playerYamlFromHelper, loadedYamlPlayer, registry), "Expected reflected player YAML to deserialize");
	Require(loadedYamlPlayer.Name == "Hero", "Expected player YAML name to round-trip");

	const HE::Ecs::RegisteredType* transformMetadata = registry.FindByName("TransformComponent");
	Require(transformMetadata != nullptr, "Expected TransformComponent metadata to be registered");
	Require(transformMetadata->Descriptor.Reflection != nullptr, "Expected TransformComponent metadata runtime type");
	Require(transformMetadata->Descriptor.Reflection->QualifiedName == "HE::TransformComponent", "Expected TransformComponent runtime type metadata");
	Require(transformMetadata->Descriptor.DefaultConstruct != nullptr, "Expected TransformComponent default construction");
	Require(transformMetadata->Descriptor.Destroy != nullptr, "Expected TransformComponent destruction");
	Require(transformMetadata->Descriptor.CopyConstruct != nullptr, "Expected TransformComponent copying");
	Require(transformMetadata->Descriptor.Relocate != nullptr, "Expected safe TransformComponent relocation into storage");
	Require(transformMetadata->Descriptor.Size == sizeof(HE::TransformComponent), "Expected TransformComponent native size");

	HE::TransformComponent sourceTransform;
	sourceTransform.Position = { 1.0f, 2.0f, 3.0f };
	sourceTransform.Rotation = { 4.0f, 5.0f, 6.0f };
	sourceTransform.Scale = { 7.0f, 8.0f, 9.0f };

	HE::Serialization::JsonSerializationBackend transformWriteBackend;
	HE::Serialization::TypeRegistryScope transformWriteScope(transformWriteBackend, registry);
	HE::Refl::SerializeRuntimeObject(*transformMetadata->Descriptor.Reflection, transformWriteBackend, transformMetadata->Descriptor.Name, &sourceTransform);
	const std::string transformMetadataJson = transformWriteBackend.SaveToString();
	Require(transformMetadataJson.find("\"Position\"") != std::string::npos, "Expected metadata serialization to emit Position");
	Require(transformMetadataJson.find("\"Rotation\"") != std::string::npos, "Expected metadata serialization to emit Rotation");
	Require(transformMetadataJson.find("\"Scale\"") != std::string::npos, "Expected metadata serialization to emit Scale");

	HE::TransformComponent loadedMetadataTransform;
	HE::Serialization::JsonSerializationBackend transformReadBackend;
	HE::Serialization::TypeRegistryScope transformReadScope(transformReadBackend, registry);
	transformReadBackend.LoadFromString(transformMetadataJson);
	Require(
		HE::Refl::DeserializeRuntimeObject(*transformMetadata->Descriptor.Reflection, transformReadBackend, transformMetadata->Descriptor.Name, &loadedMetadataTransform),
		"Expected metadata transform JSON to deserialize");
	Require(loadedMetadataTransform.Position == sourceTransform.Position, "Expected metadata Position to round-trip");
	Require(loadedMetadataTransform.Rotation == sourceTransform.Rotation, "Expected metadata Rotation to round-trip");
	Require(loadedMetadataTransform.Scale == sourceTransform.Scale, "Expected metadata Scale to round-trip");

	std::vector<HE::TransformComponent> transforms(3);
	transforms[0].Position = { 1.0f, 0.0f, 0.0f };
	transforms[1].Position = { 0.0f, 1.0f, 0.0f };
	transforms[2].Position = { 0.0f, 0.0f, 1.0f };

	const std::string transformsJson = HE::Serialization::ToJson(transforms, registry);
	std::vector<HE::TransformComponent> loadedTransforms;
	Require(HE::Serialization::FromJson(transformsJson, loadedTransforms, registry), "Expected vector JSON to deserialize");
	Require(loadedTransforms.size() == 3, "Expected vector size to round-trip");
	Require(loadedTransforms[2].Position.z == 1.0f, "Expected vector element payload to round-trip");

	HE::Serialization::YamlSerializationBackend yamlWriteBackend;
	yamlWriteBackend.BeginObject("player");
	yamlWriteBackend.Serialize("name", std::string("Hero"));
	yamlWriteBackend.Serialize("level", static_cast<int32_t>(10));
	yamlWriteBackend.BeginArray("inventory", 3);
	for (size_t i = 0; i < player.Inventory.size(); ++i) {
		yamlWriteBackend.BeginArrayElement(i);
		yamlWriteBackend.Serialize("", player.Inventory[i]);
		yamlWriteBackend.EndArrayElement();
	}
	yamlWriteBackend.EndArray();
	yamlWriteBackend.EndObject();

	const std::string playerYaml = yamlWriteBackend.SaveToString();
	Require(playerYaml.find("player") != std::string::npos, "Expected YAML backend to emit object field");

	HE::Serialization::YamlSerializationBackend yamlReadBackend;
	yamlReadBackend.LoadFromString(playerYaml);
	Require(yamlReadBackend.HasField("player"), "Expected YAML backend to find root object field");
	Require(
		yamlReadBackend.GetFieldType("player") == HE::Serialization::SerializationType::Object,
		"Expected YAML backend to report object field type");
	yamlReadBackend.BeginObject("player");
	Require(yamlReadBackend.GetArraySize("inventory") == 3, "Expected YAML backend array size to round-trip");
	Require(
		yamlReadBackend.GetFieldType("inventory") == HE::Serialization::SerializationType::Array,
		"Expected YAML backend to report array field type");
	std::string yamlName;
	int32_t yamlLevel = 0;
	Require(yamlReadBackend.Deserialize("name", yamlName), "Expected YAML backend string field to deserialize");
	Require(yamlReadBackend.Deserialize("level", yamlLevel), "Expected YAML backend int field to deserialize");
	Require(yamlName == "Hero", "Expected YAML backend string field to round-trip");
	Require(yamlLevel == 10, "Expected YAML backend int field to round-trip");
	yamlReadBackend.BeginArray("inventory");
	yamlReadBackend.BeginArrayElement(1);
	std::string yamlInventoryItem;
	Require(yamlReadBackend.Deserialize("", yamlInventoryItem), "Expected YAML backend array scalar element to deserialize");
	yamlReadBackend.EndArrayElement();
	yamlReadBackend.EndArray();
	Require(yamlInventoryItem == "shield", "Expected YAML backend array scalar element to round-trip");
	bool sawLevelViaIteration = false;
	yamlReadBackend.ForEachField([&](const std::string& key) {
		if (key == "level") {
			int32_t iteratedLevel = 0;
			sawLevelViaIteration = yamlReadBackend.Deserialize("", iteratedLevel) && iteratedLevel == 10;
		}
	});
	Require(sawLevelViaIteration, "Expected YAML backend ForEachField to expose current value context");
	yamlReadBackend.EndObject();

	HE::Scene scene("SerializationSmokeScene");
	auto entity = ECSTestSupport::Take(scene.CreateEntity("Serialized Entity"));
	ECSTestSupport::Get<HE::TransformComponent>(scene.GetWorld(), entity).Position = { 3.0f, 4.0f, 5.0f };

	const auto uuid = scene.GetWorld().Uuid(entity);
	const auto scenePath = std::filesystem::temp_directory_path() / "HuaEngineSerializationSmoke.scene";
	Require(HE::Serialization::SaveScene(scene, scenePath.string()), "Expected scene save to succeed");
	const std::string savedSceneText = [&]() {
		std::ifstream savedSceneFile(scenePath);
		Require(savedSceneFile.is_open(), "Expected saved scene file to be readable");
		return std::string((std::istreambuf_iterator<char>(savedSceneFile)), std::istreambuf_iterator<char>());
	}();
	Require(savedSceneText.find("name: SerializationSmokeScene") != std::string::npos, "Expected default scene save to use YAML mapping style");
	Require(savedSceneText.find("\"name\"") == std::string::npos, "Expected default scene save not to use JSON object syntax");

	HE::Scene loadedScene;
	Require(HE::Serialization::LoadScene(scenePath.string(), loadedScene), "Expected scene load to succeed");
	auto loadedEntity = loadedScene.GetWorld().Find(uuid);
	Require(loadedScene.GetWorld().IsAlive(loadedEntity), "Expected scene entity uuid to round-trip");
	Require(loadedScene.GetWorld().Name(loadedEntity) == "Serialized Entity", "Expected scene entity name to round-trip");
	Require(ECSTestSupport::Get<HE::TransformComponent>(loadedScene.GetWorld(), loadedEntity).Position == glm::vec3(3.0f, 4.0f, 5.0f), "Expected scene transform to round-trip");

	std::error_code errorCode;
	std::filesystem::remove(scenePath, errorCode);

	std::cout << "SerializationSmoke passed" << std::endl;
	return 0;
}
