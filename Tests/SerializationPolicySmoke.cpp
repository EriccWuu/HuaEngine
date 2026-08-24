#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <stdexcept>
#include <variant>

#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/Asset/AssetTypes.h"
#include "HuaEngine/Scene/Scene.h"
#include "HuaEngine/Scene/SceneSerializer.h"
#include "HuaEngine/Serialization/Serialization.h"
#include "Module/Rendering/RenderingComponent.h"

namespace HE {
	struct PolicyRefPayload {
		std::string Name = "payload-default";
		int Value = 0;
	};

	struct PolicyRefHolder {
		Ref<PolicyRefPayload> Payload;
	};

	struct PolicyReflectedComponent {
		std::string Name = "default-name";
		int Level = 7;
		float Health = 100.0f;
	};
}

srefl_class(HE::PolicyRefPayload,
	fields(
		field(Name),
		field(Value)
	)
)

srefl_class(HE::PolicyRefHolder,
	fields(
		field(Payload)
	)
)

srefl_class(HE::PolicyReflectedComponent,
	fields(
		field(Name),
		field(Level),
		field(Health)
	)
)

namespace {
	std::filesystem::path g_TempDirectory;

	void Require(bool condition, const std::string& message) {
		if (!condition) {
			throw std::runtime_error(message);
		}
	}

	std::filesystem::path CreateUniqueTempDirectory() {
		const auto tempRoot = std::filesystem::temp_directory_path();
		const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
		const auto addressSalt = reinterpret_cast<std::uintptr_t>(&tempRoot);

		for (uint32_t attempt = 0; attempt < 32; ++attempt) {
			const auto candidate = tempRoot / (
				"HuaEngineSerializationPolicySmoke_" +
				std::to_string(timestamp) + "_" +
				std::to_string(addressSalt) + "_" +
				std::to_string(attempt));

			std::error_code errorCode;
			if (std::filesystem::create_directory(candidate, errorCode)) {
				return candidate;
			}
		}

		throw std::runtime_error("Expected unique test temp directory to be creatable");
	}

	void CleanupTempDirectory() {
		if (g_TempDirectory.empty()) {
			return;
		}

		std::error_code errorCode;
		std::filesystem::remove_all(g_TempDirectory, errorCode);
		g_TempDirectory.clear();
	}

	std::filesystem::path MakePolicyPath(const std::string& filename) {
		Require(!g_TempDirectory.empty(), "Expected test temp directory to be initialized");
		return g_TempDirectory / filename;
	}

	void WriteTextFile(const std::filesystem::path& path, const std::string& content) {
		std::ofstream file(path, std::ios::trunc);
		Require(file.is_open(), "Expected test scene file to be writable");
		file << content;
	}

	std::string ReadTextFile(const std::filesystem::path& path) {
		std::ifstream file(path);
		Require(file.is_open(), "Expected test file to be readable");
		std::ostringstream buffer;
		buffer << file.rdbuf();
		return buffer.str();
	}

	void RemoveFile(const std::filesystem::path& path) {
		std::error_code errorCode;
		std::filesystem::remove(path, errorCode);
	}

	void VerifyTransformRuntimeDeserializeRejectsPositionWithoutMutation(
		const std::string& json,
		const std::string& message) {
		const HE::Refl::RuntimeTypeDescriptor* transformDescriptor = HE::Refl::FindRuntimeType("HE::TransformComponent");
		Require(transformDescriptor != nullptr, "Expected TransformComponent runtime descriptor");

		const glm::vec3 sentinelPosition = { 9.0f, 9.0f, 9.0f };
		HE::TransformComponent target;
		target.Position = sentinelPosition;

		HE::Serialization::JsonSerializationBackend backend;
		backend.LoadFromString(json);
		Require(
			!HE::Refl::DeserializeRuntimeObject(*transformDescriptor, backend, std::string(transformDescriptor->Name), &target),
			message);
		Require(target.Position == sentinelPosition, "Expected failed runtime Position deserialize to keep original value");
	}

	void VerifyRuntimeComponentDeserializeFailureDoesNotPartiallyMutate() {
		VerifyTransformRuntimeDeserializeRejectsPositionWithoutMutation(R"({
  "TransformComponent": {
    "Position": {
      "x": 1.0,
      "y": 2.0
    }
  }
})", "Expected runtime TransformComponent deserialize to fail when Position.z is missing");

		VerifyTransformRuntimeDeserializeRejectsPositionWithoutMutation(R"({
  "TransformComponent": {
    "Position": {
      "x": 1.0,
      "y": 2.0,
      "z": "bad"
    }
  }
})", "Expected runtime TransformComponent deserialize to fail when Position.z has the wrong type");
	}

	void VerifyUnknownEnumStringFailsWithoutMutation() {
		const auto* materialDescriptor = HE::Refl::FindRuntimeType("HE::Rendering::MaterialComponent");
		Require(materialDescriptor != nullptr, "Expected MaterialComponent runtime descriptor");

		HE::Rendering::MaterialComponent material;
		material.BlendMode = HE::Rendering::MaterialBlendMode::Masked;

		HE::Serialization::JsonSerializationBackend backend;
		backend.LoadFromString("{\"MaterialComponent\":{\"BlendMode\":\"DoesNotExist\"}}");
		Require(
			!HE::Refl::DeserializeRuntimeObject(*materialDescriptor, backend, "MaterialComponent", &material),
			"Expected unknown enum string to fail deserialization");
		Require(
			material.BlendMode == HE::Rendering::MaterialBlendMode::Masked,
			"Expected failed enum deserialization not to overwrite existing enum value");
	}

	void VerifyMaterialOverrideSetBasicTypesRoundTripAndRejectsUnknownTypes() {
		HE::Rendering::MaterialOverrideSet overrides;
		overrides.Parameters["u_Roughness"] = 0.75f;
		overrides.Parameters["u_Tint"] = glm::vec3(0.1f, 0.2f, 0.3f);
		overrides.TextureParameters["u_Texture"] = "texture-guid";

		const std::string json = HE::Serialization::ToJson(overrides);
		Require(json.find("\"u_Roughness\"") != std::string::npos, "Expected float material override to serialize");
		Require(json.find("\"float\"") != std::string::npos, "Expected float material override type");
		Require(json.find("\"u_Tint\"") != std::string::npos, "Expected vec3 material override to serialize");
		Require(json.find("\"vec3\"") != std::string::npos, "Expected vec3 material override type");

		HE::Rendering::MaterialOverrideSet loaded;
		Require(HE::Serialization::FromJson(json, loaded), "Expected material overrides to deserialize");
		Require(std::holds_alternative<float>(loaded.Parameters.at("u_Roughness")), "Expected u_Roughness to deserialize as float");
		Require(std::get<float>(loaded.Parameters.at("u_Roughness")) == 0.75f, "Expected float override value to round-trip");
		Require(std::holds_alternative<glm::vec3>(loaded.Parameters.at("u_Tint")), "Expected u_Tint to deserialize as vec3");
		Require(std::get<glm::vec3>(loaded.Parameters.at("u_Tint")) == glm::vec3(0.1f, 0.2f, 0.3f), "Expected vec3 override value to round-trip");
		Require(loaded.TextureParameters.at("u_Texture") == "texture-guid", "Expected texture GUID override to round-trip");

		HE::Serialization::JsonSerializationBackend backend;
		backend.LoadFromString(R"({
  "Overrides": {
    "parameters": {
      "u_Unsupported": {
        "type": "texture2d",
        "value": "Texture.png"
      }
    }
  },
  "Sibling": true
})");
		HE::Rendering::MaterialOverrideSet rejected;
		Require(!HE::Serialization::DeserializeValue(backend, "Overrides", rejected), "Expected unknown material override type to fail");
		Require(backend.HasField("Sibling"), "Expected failed override deserialize to restore backend scope");
		Require(rejected.Parameters.empty(), "Expected failed override deserialize not to keep unsupported value");
	}

	void VerifyMissingReflectedFieldsKeepDefaults() {
		// Policy: missing reflected fields are not diagnostics yet; they deserialize successfully
		// and keep the target object's current default values until structured diagnostics exist.
		const std::string json = R"({
  "Name": "loaded-name",
  "Health": 42.0
})";

		HE::PolicyReflectedComponent component;
		component.Level = 99;
		Require(HE::Serialization::FromJson(json, component), "Expected missing reflected field JSON to deserialize");
		Require(component.Name == "loaded-name", "Expected present reflected string field to load");
		Require(component.Health == 42.0f, "Expected present reflected float field to load");
		Require(component.Level == 99, "Expected missing reflected field to keep target default value");
	}

	void VerifyUnknownFieldsAreIgnored() {
		const std::string json = R"({
  "Name": "known-name",
  "Level": 12,
  "Health": 80.5,
  "FutureField": "ignored"
})";

		HE::PolicyReflectedComponent component;
		Require(HE::Serialization::FromJson(json, component), "Expected JSON with unknown reflected field to deserialize");
		Require(component.Name == "known-name", "Expected known string field to deserialize with unknown fields present");
		Require(component.Level == 12, "Expected known int field to deserialize with unknown fields present");
		Require(component.Health == 80.5f, "Expected known float field to deserialize with unknown fields present");
	}

	void VerifyRefNullRoundTrip() {
		HE::PolicyRefHolder holder;
		holder.Payload = nullptr;

		const std::string json = HE::Serialization::ToJson(holder);
		HE::PolicyRefHolder loaded;
		loaded.Payload = HE::CreateRef<HE::PolicyRefPayload>();
		Require(HE::Serialization::FromJson(json, loaded), "Expected nullable Ref<T> JSON to deserialize");
		Require(loaded.Payload == nullptr, "Expected nullable Ref<T> null state to round-trip");
	}

	void VerifyRefNonNullRoundTrip() {
		HE::PolicyRefHolder holder;
		holder.Payload = HE::CreateRef<HE::PolicyRefPayload>();
		holder.Payload->Name = "non-null-payload";
		holder.Payload->Value = 314;

		const std::string json = HE::Serialization::ToJson(holder);
		HE::PolicyRefHolder loaded;
		Require(HE::Serialization::FromJson(json, loaded), "Expected non-null Ref<T> JSON to deserialize");
		Require(loaded.Payload != nullptr, "Expected non-null Ref<T> to allocate during deserialize");
		Require(loaded.Payload->Name == "non-null-payload", "Expected non-null Ref<T> string payload to round-trip");
		Require(loaded.Payload->Value == 314, "Expected non-null Ref<T> int payload to round-trip");
	}

	void VerifyUnknownSceneComponentIsSkipped() {
		const auto scenePath = MakePolicyPath("HuaEngineSerializationPolicyUnknownComponent.scene");
		const std::string uuid = "00000000000000000000000000000042";
		WriteTextFile(scenePath, R"(entities:
  - components:
      FutureUnknownComponent:
        Enabled: true
      TransformComponent:
        Position:
          x: 10.0
          y: 20.0
          z: 30.0
        Rotation:
          x: 1.0
          y: 2.0
          z: 3.0
        Scale:
          x: 4.0
          y: 5.0
          z: 6.0
    name: Known Entity
    uuid: '00000000000000000000000000000042'
name: Unknown Component Policy
version: 3
)");

		HE::Scene loaded;
		Require(HE::Serialization::LoadScene(scenePath.string(), loaded), "Expected scene with unknown component to load");
		HE::Entity entity = loaded.GetWorld().GetEntity(HE::EntityUuid::FromString(uuid));
		Require(entity.IsValid(), "Expected known entity to load when unknown component is skipped");
		Require(entity.HasComponent<HE::TransformComponent>(), "Expected known component to load when unknown component is skipped");
		const auto& transform = entity.GetComponent<HE::TransformComponent>();
		Require(transform.Position == glm::vec3(10.0f, 20.0f, 30.0f), "Expected known component Position to load");
		Require(transform.Rotation == glm::vec3(1.0f, 2.0f, 3.0f), "Expected known component Rotation to load");
		Require(transform.Scale == glm::vec3(4.0f, 5.0f, 6.0f), "Expected known component Scale to load");

		RemoveFile(scenePath);
	}

	void VerifyKnownSceneComponentInvalidFieldFailsLoad() {
		const auto scenePath = MakePolicyPath("HuaEngineSerializationPolicyInvalidKnownComponent.scene");
		WriteTextFile(scenePath, R"(entities:
  - components:
      TransformComponent:
        Position:
          x: 7.0
          y: 8.0
          z: bad
        Rotation:
          x: 11.0
          y: 12.0
          z: 13.0
        Scale:
          x: 1.0
          y: 1.0
          z: 1.0
    name: Invalid Known Component Entity
    uuid: '00000000000000000000000000000044'
name: Invalid Known Component Policy
version: 3
)");

		HE::Scene loaded;
		Require(!HE::Serialization::LoadScene(scenePath.string(), loaded), "Expected scene load to fail when a known component field is invalid");

		RemoveFile(scenePath);
	}

	void VerifyKnownSceneComponentNonObjectFailsLoad() {
		const auto scenePath = MakePolicyPath("HuaEngineSerializationPolicyKnownComponentNonObject.scene");
		WriteTextFile(scenePath, R"(entities:
  - components:
      TransformComponent: 123
    name: Invalid Known Component Shape Entity
    uuid: '00000000000000000000000000000046'
name: Invalid Known Component Shape Policy
version: 3
)");

		HE::Scene loaded;
		Require(!HE::Serialization::LoadScene(scenePath.string(), loaded), "Expected scene load to fail when a known component payload is not an object");

		RemoveFile(scenePath);
	}

	void VerifyInvalidMaterialComponentDoesNotPoisonNextSceneLoad() {
		const auto invalidScenePath = MakePolicyPath("HuaEngineSerializationPolicyInvalidMaterialComponent.scene");
		WriteTextFile(invalidScenePath, R"(entities:
  - components:
      MaterialComponent:
        BlendMode: Opaque
        Material:
          guid: builtin-material-default
        Overrides:
          parameters:
            u_Unsupported:
              type: texture2d
              value: Texture.png
      TransformComponent:
        Position:
          x: 1.0
          y: 2.0
          z: 3.0
        Rotation:
          x: 4.0
          y: 5.0
          z: 6.0
        Scale:
          x: 1.0
          y: 1.0
          z: 1.0
    name: Invalid Material Entity
    uuid: '00000000000000000000000000000045'
name: Invalid Material Component Policy
version: 3
)");

		HE::Scene invalidLoaded;
		Require(!HE::Serialization::LoadScene(invalidScenePath.string(), invalidLoaded), "Expected invalid MaterialComponent scene load to fail");

		const auto validScenePath = MakePolicyPath("HuaEngineSerializationPolicyValidAfterMaterialFailure.scene");
		const std::string uuid = "00000000000000000000000000000046";
		WriteTextFile(validScenePath, R"(entities:
  - components:
      MaterialComponent:
        BlendMode: Transparent
        Material:
          guid: builtin-material-default
        Overrides:
          parameters:
            u_Roughness:
              type: float
              value: 0.65
      TransformComponent:
        Position:
          x: 10.0
          y: 20.0
          z: 30.0
        Rotation:
          x: 1.0
          y: 2.0
          z: 3.0
        Scale:
          x: 4.0
          y: 5.0
          z: 6.0
    name: Valid After Failure Entity
    uuid: '00000000000000000000000000000046'
name: Valid After Material Failure Policy
version: 3
)");

		HE::Scene validLoaded;
		Require(HE::Serialization::LoadScene(validScenePath.string(), validLoaded), "Expected later independent scene load to succeed after invalid MaterialComponent rejection");
		HE::Entity entity = validLoaded.GetWorld().GetEntity(HE::EntityUuid::FromString(uuid));
		Require(entity.IsValid(), "Expected valid follow-up scene entity to load");
		Require(entity.HasComponent<HE::TransformComponent>(), "Expected valid follow-up scene transform to load");
		Require(entity.HasComponent<HE::Rendering::MaterialComponent>(), "Expected valid follow-up scene material to load");
		const auto& material = entity.GetComponent<HE::Rendering::MaterialComponent>();
		Require(material.Material.Reference.Guid == HE::BuiltinAssetGuids::DefaultMaterial, "Expected valid follow-up scene material GUID to load");
		Require(material.BlendMode == HE::Rendering::MaterialBlendMode::Transparent, "Expected valid follow-up scene blend mode to load");
		Require(material.Overrides.Parameters.find("u_Roughness") != material.Overrides.Parameters.end(), "Expected valid follow-up scene material override to load");
		Require(std::get<float>(material.Overrides.Parameters.at("u_Roughness")) == 0.65f, "Expected valid follow-up scene material override value to load");

		RemoveFile(invalidScenePath);
		RemoveFile(validScenePath);
	}

	void VerifyMissingSceneComponentFieldKeepsDefault() {
		const auto scenePath = MakePolicyPath("HuaEngineSerializationPolicyMissingComponentField.scene");
		const std::string uuid = "00000000000000000000000000000043";
		WriteTextFile(scenePath, R"(entities:
  - components:
      TransformComponent:
        Position:
          x: 7.0
          y: 8.0
          z: 9.0
        Rotation:
          x: 11.0
          y: 12.0
          z: 13.0
    name: Missing Field Entity
    uuid: '00000000000000000000000000000043'
name: Missing Field Policy
version: 3
)");

		HE::Scene loaded;
		Require(HE::Serialization::LoadScene(scenePath.string(), loaded), "Expected scene with missing reflected component field to load");
		HE::Entity entity = loaded.GetWorld().GetEntity(HE::EntityUuid::FromString(uuid));
		Require(entity.IsValid(), "Expected entity with missing component field to load");
		Require(entity.HasComponent<HE::TransformComponent>(), "Expected component with missing reflected field to load");
		const auto& transform = entity.GetComponent<HE::TransformComponent>();
		Require(transform.Position == glm::vec3(7.0f, 8.0f, 9.0f), "Expected present component Position to load");
		Require(transform.Rotation == glm::vec3(11.0f, 12.0f, 13.0f), "Expected present component Rotation to load");
		Require(transform.Scale == glm::vec3(1.0f, 1.0f, 1.0f), "Expected missing component Scale to keep default");

		RemoveFile(scenePath);
	}

	void VerifySceneOutputStability() {
		HE::Scene scene("Stable Output Policy");
		auto first = scene.GetWorld().CreateEntity("First Entity");
		first.GetComponent<HE::TransformComponent>().Position = { 1.0f, 2.0f, 3.0f };
		auto second = scene.GetWorld().CreateEntity("Second Entity");
		second.GetComponent<HE::TransformComponent>().Rotation = { 4.0f, 5.0f, 6.0f };

		const auto firstPath = MakePolicyPath("HuaEngineSerializationPolicyStableA.scene");
		const auto secondPath = MakePolicyPath("HuaEngineSerializationPolicyStableB.scene");
		Require(HE::Serialization::SaveScene(scene, firstPath.string()), "Expected first scene save to succeed");
		Require(HE::Serialization::SaveScene(scene, secondPath.string()), "Expected second scene save to succeed");

		const std::string firstText = ReadTextFile(firstPath);
		const std::string secondText = ReadTextFile(secondPath);
		Require(firstText == secondText, "Expected consecutive scene serialization output to be stable");

		RemoveFile(firstPath);
		RemoveFile(secondPath);
	}
}

int main() {
	try {
		g_TempDirectory = CreateUniqueTempDirectory();

		HE::Log::Init({ .EnableConsoleOutput = false });
		HE::Serialization::InitializeSerialization();

		VerifyMissingReflectedFieldsKeepDefaults();
		VerifyUnknownFieldsAreIgnored();
		VerifyRefNullRoundTrip();
		VerifyRefNonNullRoundTrip();
		VerifyMaterialOverrideSetBasicTypesRoundTripAndRejectsUnknownTypes();
		VerifyRuntimeComponentDeserializeFailureDoesNotPartiallyMutate();
		VerifyUnknownEnumStringFailsWithoutMutation();
		VerifyUnknownSceneComponentIsSkipped();
		VerifyKnownSceneComponentInvalidFieldFailsLoad();
		VerifyKnownSceneComponentNonObjectFailsLoad();
		VerifyInvalidMaterialComponentDoesNotPoisonNextSceneLoad();
		VerifyMissingSceneComponentFieldKeepsDefault();
		VerifySceneOutputStability();

		CleanupTempDirectory();
		std::cout << "SerializationPolicySmoke passed" << std::endl;
		return 0;
	}
	catch (const std::exception& exception) {
		std::cerr << "[SerializationPolicySmoke] " << exception.what() << std::endl;
		CleanupTempDirectory();
		return 1;
	}
}
