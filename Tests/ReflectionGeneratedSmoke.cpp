#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <string_view>

#include "HuaEngine/ECS/ComponentRegistry.h"
#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/Generated/GeneratedReflection.h"
#include "HuaEngine/Reflection/Reflection.h"
#include "HuaEngine/Serialization/Serialization.h"
#include "Module/Rendering/RenderingComponent.h"

namespace {
	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[ReflectionGeneratedSmoke] " << message << std::endl;
			std::exit(1);
		}
	}

	bool HasField(std::span<const HE::Generated::ReflectedFieldInfo> fields, std::string_view name) {
		for (const HE::Generated::ReflectedFieldInfo& field : fields) {
			if (field.Name == name) {
				return true;
			}
		}

		return false;
	}

	const HE::Refl::RuntimeFieldDescriptor* FindRuntimeField(
		std::span<const HE::Refl::RuntimeFieldDescriptor> fields,
		std::string_view name) {
		for (const HE::Refl::RuntimeFieldDescriptor& field : fields) {
			if (field.Name == name) {
				return &field;
			}
		}

		return nullptr;
	}
}

int main() {
	const std::span<const HE::Generated::ReflectedTypeInfo> types = HE::Generated::GetReflectedTypes();
	Require(types.size() == 5, "Expected five reflected component types");

	const HE::Generated::ReflectedTypeInfo* transform = HE::Generated::FindReflectedType("HE::TransformComponent");
	Require(transform != nullptr, "Expected to find HE::TransformComponent");
	Require(HasField(transform->Fields, "Position"), "Expected TransformComponent Position field");
	Require(HasField(transform->Fields, "Rotation"), "Expected TransformComponent Rotation field");
	Require(HasField(transform->Fields, "Scale"), "Expected TransformComponent Scale field");

	const auto* transformRuntime = HE::Refl::FindRuntimeType("HE::TransformComponent");
	Require(transformRuntime != nullptr, "Expected runtime reflection descriptor for TransformComponent");
	Require(transformRuntime->Fields.size() == 3, "Expected TransformComponent runtime fields");
	Require(transformRuntime->Serialize == nullptr, "Expected TransformComponent to use generic runtime serialization");
	Require(transformRuntime->Deserialize == nullptr, "Expected TransformComponent to use generic runtime deserialization");
	const HE::Refl::RuntimeFieldDescriptor* positionField = FindRuntimeField(transformRuntime->Fields, "Position");
	Require(positionField != nullptr, "Expected TransformComponent runtime Position field");
	Require(FindRuntimeField(transformRuntime->Fields, "Rotation") != nullptr, "Expected TransformComponent runtime Rotation field");
	Require(FindRuntimeField(transformRuntime->Fields, "Scale") != nullptr, "Expected TransformComponent runtime Scale field");
	Require(positionField->Offset == offsetof(HE::TransformComponent, Position), "Expected Position runtime offset");
	Require(positionField->Size == sizeof(glm::vec3), "Expected Position runtime field size");
	Require(HE::Refl::HasRuntimeFieldFlag(positionField->Flags, HE::Refl::RuntimeFieldFlags::Serializable), "Expected Position to be serializable");
	Require(HE::Refl::HasRuntimeFieldFlag(positionField->Flags, HE::Refl::RuntimeFieldFlags::ComponentField), "Expected Position to be a component field");
	Require(positionField->GetConst != nullptr, "Expected Position const accessor");
	Require(positionField->GetMutable != nullptr, "Expected Position mutable accessor");
	Require(positionField->Serialize != nullptr, "Expected Position runtime field serializer");
	Require(positionField->Deserialize != nullptr, "Expected Position runtime field deserializer");
	Require(
		HE::Refl::GetRuntimeFieldValueKind(*positionField) == HE::Refl::RuntimeFieldValueKind::Float3,
		"Expected Position to be classified as Float3");
	Require(HE::Refl::IsRuntimeFieldSerializable(*positionField), "Expected Position to be serializable through helper");
	Require(HE::Refl::IsRuntimeFieldEditable(*positionField), "Expected Position to be editable through helper");

	const HE::Generated::ReflectedTypeInfo* mesh = HE::Generated::FindReflectedType("HE::Rendering::MeshComponent");
	Require(mesh != nullptr, "Expected to find HE::Rendering::MeshComponent");
	Require(HasField(mesh->Fields, "MeshAssetName"), "Expected MeshComponent MeshAssetName field");
	Require(!HasField(mesh->Fields, "m_CachedVertexArray"), "Expected MeshComponent cache field to be omitted");

	HE::ComponentRegistry registry;
	HE::Generated::RegisterGeneratedComponents(registry);

	constexpr std::string_view expectedNames[] = {
		"NameComponent",
		"TransformComponent",
		"CameraComponent",
		"MeshComponent",
		"MaterialComponent",
	};
	for (const std::string_view name : expectedNames) {
		Require(registry.FindByName(name) != nullptr, "Expected registered component: " + std::string(name));
	}

	const HE::ComponentMetadata* transformMetadata = registry.FindByName("TransformComponent");
	Require(transformMetadata != nullptr, "Expected TransformComponent metadata to be registered");
	Require(transformMetadata->Size == sizeof(HE::TransformComponent), "Expected TransformComponent metadata size to match component size");
	Require(transformMetadata->RuntimeType == transformRuntime, "Expected TransformComponent metadata to reference runtime type");

	HE::TransformComponent sourceTransform;
	sourceTransform.Position = { 1.0f, 2.0f, 3.0f };
	sourceTransform.Rotation = { 4.0f, 5.0f, 6.0f };
	sourceTransform.Scale = { 7.0f, 8.0f, 9.0f };

	HE::Serialization::JsonSerializationBackend writeBackend;
	HE::Refl::SerializeRuntimeObject(*transformMetadata->RuntimeType, writeBackend, transformMetadata->TypeName, &sourceTransform);
	const std::string transformJson = writeBackend.SaveToString();
	Require(transformJson.find("\"Position\"") != std::string::npos, "Expected metadata serialization to emit Position");
	Require(transformJson.find("\"Rotation\"") != std::string::npos, "Expected metadata serialization to emit Rotation");
	Require(transformJson.find("\"Scale\"") != std::string::npos, "Expected metadata serialization to emit Scale");

	HE::TransformComponent loadedTransform;
	HE::Serialization::JsonSerializationBackend readBackend;
	readBackend.LoadFromString(transformJson);
	Require(
		HE::Refl::DeserializeRuntimeObject(*transformMetadata->RuntimeType, readBackend, transformMetadata->TypeName, &loadedTransform),
		"Expected metadata deserialization to succeed");
	Require(loadedTransform.Position == sourceTransform.Position, "Expected metadata deserialization to round-trip Position");
	Require(loadedTransform.Rotation == sourceTransform.Rotation, "Expected metadata deserialization to round-trip Rotation");
	Require(loadedTransform.Scale == sourceTransform.Scale, "Expected metadata deserialization to round-trip Scale");

	glm::vec3 readPosition{};
	Require(
		HE::Refl::GetRuntimeFieldValue(*positionField, &sourceTransform, readPosition),
		"Expected generic runtime get to read Position");
	Require(readPosition == sourceTransform.Position, "Expected generic runtime get to preserve Position value");

	glm::vec3 updatedPosition{ 10.0f, 11.0f, 12.0f };
	Require(
		HE::Refl::SetRuntimeFieldValue(*positionField, &loadedTransform, updatedPosition),
		"Expected generic runtime set to write Position");
	Require(loadedTransform.Position == updatedPosition, "Expected generic runtime set to update Position");

	const auto* blendModeEnum = HE::Refl::FindRuntimeEnum("HE::Rendering::MaterialBlendMode");
	Require(blendModeEnum != nullptr, "Expected MaterialBlendMode runtime enum");
	Require(blendModeEnum->Values.size() == 3, "Expected three MaterialBlendMode values");
	Require(
		HE::Refl::FindRuntimeEnumValueByName(*blendModeEnum, "Transparent") != nullptr,
		"Expected Transparent enum value");
	Require(
		HE::Refl::FindRuntimeEnumValueByValue(*blendModeEnum, static_cast<int64_t>(HE::Rendering::MaterialBlendMode::Masked)) != nullptr,
		"Expected Masked enum value by integer value");

	const HE::Generated::ReflectedEnumInfo* generatedBlendMode =
		HE::Generated::FindReflectedEnum("HE::Rendering::MaterialBlendMode");
	Require(generatedBlendMode != nullptr, "Expected generated MaterialBlendMode enum info");

	const auto* materialRuntime = HE::Refl::FindRuntimeType("HE::Rendering::MaterialComponent");
	Require(materialRuntime != nullptr, "Expected MaterialComponent runtime descriptor");
	const HE::Refl::RuntimeFieldDescriptor* blendModeField = FindRuntimeField(materialRuntime->Fields, "BlendMode");
	Require(blendModeField != nullptr, "Expected MaterialComponent BlendMode field");
	Require(blendModeField->EnumType == blendModeEnum, "Expected BlendMode field to reference enum metadata");
	Require(
		HE::Refl::GetRuntimeFieldValueKind(*blendModeField) == HE::Refl::RuntimeFieldValueKind::Enum,
		"Expected BlendMode to be classified as enum");

	HE::Rendering::MaterialComponent materialComponent;
	Require(
		HE::Refl::SetRuntimeEnumFieldValueByName(*blendModeField, &materialComponent, "Transparent"),
		"Expected generic enum set by name to succeed");
	int64_t enumRuntimeValue = 0;
	Require(
		HE::Refl::GetRuntimeEnumFieldValue(*blendModeField, &materialComponent, enumRuntimeValue),
		"Expected generic enum get to succeed");
	Require(
		enumRuntimeValue == static_cast<int64_t>(HE::Rendering::MaterialBlendMode::Transparent),
		"Expected enum runtime value to match Transparent");

	HE::Serialization::JsonSerializationBackend materialWriteBackend;
	HE::Refl::SerializeRuntimeObject(*materialRuntime, materialWriteBackend, "MaterialComponent", &materialComponent);
	const std::string materialJson = materialWriteBackend.SaveToString();
	Require(materialJson.find("\"BlendMode\": \"Transparent\"") != std::string::npos, "Expected enum to serialize as string name");

	HE::Rendering::MaterialComponent loadedMaterial;
	HE::Serialization::JsonSerializationBackend materialReadBackend;
	materialReadBackend.LoadFromString(materialJson);
	Require(
		HE::Refl::DeserializeRuntimeObject(*materialRuntime, materialReadBackend, "MaterialComponent", &loadedMaterial),
		"Expected enum string deserialization to succeed");
	Require(loadedMaterial.BlendMode == HE::Rendering::MaterialBlendMode::Transparent, "Expected enum round-trip");

	std::cout << "ReflectionGeneratedSmoke passed" << std::endl;
	return 0;
}
