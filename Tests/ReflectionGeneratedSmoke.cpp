#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <string_view>

#include "HuaEngine/ECS/Runtime/EcsContext.h"
#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/Generated/GeneratedEcs.h"
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
	HE::Ecs::EcsContext context;
	auto& registry = context.Types();
	Require(HE::Generated::RegisterGeneratedComponents(registry).HasValue(), "Expected component metadata registration");
	Require(registry.All().size() == 4, "Expected four reflected component types");
	Require(registry.FindByQualifiedName("HE::NameComponent") == nullptr, "Expected NameComponent reflection to be absent");

	const auto* transformMetadata = registry.FindByQualifiedName("HE::TransformComponent");
	Require(transformMetadata != nullptr, "Expected to find HE::TransformComponent");
	const auto* transformRuntime = transformMetadata->Descriptor.Reflection;
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
	Require(positionField->GetConst != nullptr, "Expected Position const accessor");
	Require(positionField->GetMutable != nullptr, "Expected Position mutable accessor");
	Require(positionField->Serialize != nullptr, "Expected Position runtime field serializer");
	Require(positionField->Deserialize != nullptr, "Expected Position runtime field deserializer");
	Require(
		HE::Refl::GetRuntimeFieldValueKind(*positionField) == HE::Refl::RuntimeFieldValueKind::Float3,
		"Expected Position to be classified as Float3");
	Require(HE::Refl::IsRuntimeFieldSerializable(*positionField), "Expected Position to be serializable through helper");
	Require(HE::Refl::IsRuntimeFieldEditable(*positionField), "Expected Position to be editable through helper");

	const auto* mesh = registry.FindByQualifiedName("HE::Rendering::MeshComponent");
	Require(mesh && mesh->Descriptor.Reflection, "Expected MeshComponent runtime reflection");
	Require(FindRuntimeField(mesh->Descriptor.Reflection->Fields, "Mesh") != nullptr, "Expected MeshComponent Mesh field");
	Require(FindRuntimeField(mesh->Descriptor.Reflection->Fields, "MeshAssetName") == nullptr, "Expected MeshAssetName to be absent");
	Require(FindRuntimeField(mesh->Descriptor.Reflection->Fields, "m_CachedVertexArray") == nullptr,
		"Expected MeshComponent cache field to be omitted");
	const auto* camera = registry.FindByQualifiedName("HE::Rendering::CameraComponent");
	Require(camera && camera->Descriptor.Reflection &&
		camera->Descriptor.Guid == camera->Descriptor.Reflection->Guid,
		"Expected camera reflection and ECS to share one stable Guid");
	const auto* fov = FindRuntimeField(camera->Descriptor.Reflection->Fields, "VerticalFovDegrees");
	const auto* minimum = fov ? HE::Refl::FindRuntimeAttribute(fov->Attributes, "Editor.Min") : nullptr;
	const auto* maximum = fov ? HE::Refl::FindRuntimeAttribute(fov->Attributes, "Editor.Max") : nullptr;
	Require(minimum && minimum->Value == "1" && maximum && maximum->Value == "179",
		"Expected camera editor bounds in extensible field attributes");

	const auto* material = registry.FindByQualifiedName("HE::Rendering::MaterialComponent");
	Require(material && material->Descriptor.Reflection, "Expected MaterialComponent runtime reflection");
	Require(FindRuntimeField(material->Descriptor.Reflection->Fields, "Material") != nullptr, "Expected MaterialComponent Material field");
	Require(FindRuntimeField(material->Descriptor.Reflection->Fields, "Overrides") != nullptr, "Expected MaterialComponent Overrides field");
	Require(FindRuntimeField(material->Descriptor.Reflection->Fields, "MaterialInstance") == nullptr,
		"Expected MaterialInstance field to be absent");

	constexpr std::string_view expectedNames[] = {
		"TransformComponent",
		"CameraComponent",
		"MeshComponent",
		"MaterialComponent",
	};
	for (const std::string_view name : expectedNames) {
		Require(registry.FindByName(name) != nullptr, "Expected registered component: " + std::string(name));
	}
	Require(registry.FindByName("NameComponent") == nullptr, "Expected NameComponent not to be registered");

	Require(transformMetadata == registry.FindByName("TransformComponent"), "Expected one Context type identity");
	Require(transformMetadata->Descriptor.Size == sizeof(HE::TransformComponent), "Expected TransformComponent metadata size to match component size");
	Require(transformMetadata->Descriptor.Reflection == transformRuntime, "Expected TransformComponent metadata to reference runtime type");

	HE::TransformComponent sourceTransform;
	sourceTransform.Position = { 1.0f, 2.0f, 3.0f };
	sourceTransform.Rotation = { 4.0f, 5.0f, 6.0f };
	sourceTransform.Scale = { 7.0f, 8.0f, 9.0f };

	HE::Serialization::JsonSerializationBackend writeBackend;
	HE::Refl::SerializeRuntimeObject(*transformMetadata->Descriptor.Reflection, writeBackend, transformMetadata->Descriptor.Name, &sourceTransform);
	const std::string transformJson = writeBackend.SaveToString();
	Require(transformJson.find("\"Position\"") != std::string::npos, "Expected metadata serialization to emit Position");
	Require(transformJson.find("\"Rotation\"") != std::string::npos, "Expected metadata serialization to emit Rotation");
	Require(transformJson.find("\"Scale\"") != std::string::npos, "Expected metadata serialization to emit Scale");

	HE::TransformComponent loadedTransform;
	HE::Serialization::JsonSerializationBackend readBackend;
	readBackend.LoadFromString(transformJson);
	Require(
		HE::Refl::DeserializeRuntimeObject(*transformMetadata->Descriptor.Reflection, readBackend, transformMetadata->Descriptor.Name, &loadedTransform),
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

	const auto* blendModeEnum = registry.FindEnum("HE::Rendering::MaterialBlendMode");
	Require(blendModeEnum != nullptr, "Expected MaterialBlendMode runtime enum");
	Require(blendModeEnum->Guid == HE::Refl::TypeGuid::FromString("4f745e86ab69460db41dac991f79c005") &&
		context.Reflection().FindEnum(blendModeEnum->Guid) == blendModeEnum,
		"Expected stable enum identity in the reflection registry");
	Require(blendModeEnum->Values.size() == 3, "Expected three MaterialBlendMode values");
	Require(
		HE::Refl::FindRuntimeEnumValueByName(*blendModeEnum, "Transparent") != nullptr,
		"Expected Transparent enum value");
	Require(
		HE::Refl::FindRuntimeEnumValueByValue(*blendModeEnum, static_cast<int64_t>(HE::Rendering::MaterialBlendMode::Masked)) != nullptr,
		"Expected Masked enum value by integer value");

	const auto* materialRuntime = material->Descriptor.Reflection;
	Require(materialRuntime != nullptr, "Expected MaterialComponent runtime descriptor");
	Require(FindRuntimeField(materialRuntime->Fields, "Material") != nullptr, "Expected MaterialComponent runtime Material field");
	Require(FindRuntimeField(materialRuntime->Fields, "Overrides") != nullptr, "Expected MaterialComponent runtime Overrides field");
	Require(FindRuntimeField(materialRuntime->Fields, "MaterialInstance") == nullptr, "Expected MaterialInstance runtime field to be removed");
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
