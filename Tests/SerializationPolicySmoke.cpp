#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/Scene/Scene.h"
#include "HuaEngine/Scene/SceneSerializer.h"
#include "HuaEngine/Serialization/Serialization.h"

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
	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[SerializationPolicySmoke] " << message << std::endl;
			std::exit(1);
		}
	}

	std::filesystem::path MakePolicyPath(const std::string& filename) {
		return std::filesystem::temp_directory_path() / filename;
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
		WriteTextFile(scenePath, R"({
  "entities": [
    {
      "components": {
        "FutureUnknownComponent": {
          "Enabled": true
        },
        "TransformComponent": {
          "Position": {
            "x": 10.0,
            "y": 20.0,
            "z": 30.0
          },
          "Rotation": {
            "x": 1.0,
            "y": 2.0,
            "z": 3.0
          },
          "Scale": {
            "x": 4.0,
            "y": 5.0,
            "z": 6.0
          }
        }
      },
      "name": "Known Entity",
      "uuid": "00000000000000000000000000000042"
    }
  ],
  "name": "Unknown Component Policy",
  "version": 3
})");

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

	void VerifyMissingSceneComponentFieldKeepsDefault() {
		const auto scenePath = MakePolicyPath("HuaEngineSerializationPolicyMissingComponentField.scene");
		const std::string uuid = "00000000000000000000000000000043";
		WriteTextFile(scenePath, R"({
  "entities": [
    {
      "components": {
        "TransformComponent": {
          "Position": {
            "x": 7.0,
            "y": 8.0,
            "z": 9.0
          },
          "Rotation": {
            "x": 11.0,
            "y": 12.0,
            "z": 13.0
          }
        }
      },
      "name": "Missing Field Entity",
      "uuid": "00000000000000000000000000000043"
    }
  ],
  "name": "Missing Field Policy",
  "version": 3
})");

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
	HE::Log::Init({ .EnableConsoleOutput = false });
	HE::Serialization::InitializeSerialization();

	VerifyMissingReflectedFieldsKeepDefaults();
	VerifyUnknownFieldsAreIgnored();
	VerifyRefNullRoundTrip();
	VerifyRefNonNullRoundTrip();
	VerifyUnknownSceneComponentIsSkipped();
	VerifyMissingSceneComponentFieldKeepsDefault();
	VerifySceneOutputStability();

	std::cout << "SerializationPolicySmoke passed" << std::endl;
	return 0;
}
