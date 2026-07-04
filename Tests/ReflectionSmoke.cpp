#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <string_view>

#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/Reflection/Reflection.h"
#include "HuaEngine/Serialization/Serialization.h"

namespace {
	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[ReflectionSmoke] " << message << std::endl;
			std::exit(1);
		}
	}

	bool HasRuntimeField(
		std::span<const HE::Refl::RuntimeFieldDescriptor> fields,
		std::string_view name,
		std::string_view type) {
		for (const HE::Refl::RuntimeFieldDescriptor& field : fields) {
			if (field.Name == name && field.Type == type) {
				return true;
			}
		}

		return false;
	}
}

int main() {
	const HE::Refl::RuntimeTypeDescriptor* transformType = HE::Refl::FindRuntimeType("HE::TransformComponent");
	Require(transformType != nullptr, "Expected runtime descriptor for HE::TransformComponent");
	Require(transformType->Name == "TransformComponent", "Expected TransformComponent runtime descriptor name");
	Require(transformType->Kind == "component", "Expected TransformComponent runtime descriptor kind");
	Require(transformType->DisplayName == "Transform", "Expected TransformComponent runtime display name");
	Require(transformType->Category == "Core", "Expected TransformComponent runtime category");
	Require(transformType->TypeId == HE::ComponentTypeIdOf<HE::TransformComponent>(), "Expected TransformComponent runtime type id");
	Require(transformType->Size == sizeof(HE::TransformComponent), "Expected TransformComponent runtime size");
	Require(transformType->Fields.size() == 3, "Expected TransformComponent runtime descriptor to expose three fields");
	Require(HasRuntimeField(transformType->Fields, "Position", "glm::vec3"), "Expected runtime Position field");
	Require(HasRuntimeField(transformType->Fields, "Rotation", "glm::vec3"), "Expected runtime Rotation field");
	Require(HasRuntimeField(transformType->Fields, "Scale", "glm::vec3"), "Expected runtime Scale field");
	for (const HE::Refl::RuntimeFieldDescriptor& field : transformType->Fields) {
		Require(field.Serialize != nullptr, "Expected runtime field serializer");
		Require(field.Deserialize != nullptr, "Expected runtime field deserializer");
	}

	HE::TransformComponent sourceTransform;
	sourceTransform.Position = { 1.0f, 2.0f, 3.0f };
	sourceTransform.Rotation = { 4.0f, 5.0f, 6.0f };
	sourceTransform.Scale = { 7.0f, 8.0f, 9.0f };

	HE::Serialization::JsonSerializationBackend writeBackend;
	HE::Refl::SerializeRuntimeObject(*transformType, writeBackend, std::string(transformType->Name), &sourceTransform);
	const std::string transformJson = writeBackend.SaveToString();
	Require(transformJson.find("\"Position\"") != std::string::npos, "Expected runtime serialization to emit Position");
	Require(transformJson.find("\"Rotation\"") != std::string::npos, "Expected runtime serialization to emit Rotation");
	Require(transformJson.find("\"Scale\"") != std::string::npos, "Expected runtime serialization to emit Scale");

	HE::TransformComponent loadedTransform;
	HE::Serialization::JsonSerializationBackend readBackend;
	readBackend.LoadFromString(transformJson);
	Require(
		HE::Refl::DeserializeRuntimeObject(*transformType, readBackend, std::string(transformType->Name), &loadedTransform),
		"Expected runtime deserialization to succeed");
	Require(loadedTransform.Position == sourceTransform.Position, "Expected runtime Position to round-trip");
	Require(loadedTransform.Rotation == sourceTransform.Rotation, "Expected runtime Rotation to round-trip");
	Require(loadedTransform.Scale == sourceTransform.Scale, "Expected runtime Scale to round-trip");

	std::cout << "ReflectionSmoke passed" << std::endl;
	return 0;
}
