#include "ECSTestSupport.h"
#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/ECS/Runtime/WorldScope.h"
#include "HuaEngine/Asset/AssetTypes.h"
#include "HuaEngine/Scene/Scene.h"
#include "HuaEngine/Scene/SceneSerializer.h"
#include "Module/Rendering/RenderingComponent.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <variant>

namespace {
	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[ECSSceneSerializationSmoke] " << message << std::endl;
			std::exit(1);
		}
	}

	const HE::Refl::RuntimeFieldDescriptor* FindField(
		const HE::Refl::RuntimeTypeDescriptor& type, std::string_view name) {
		for (const auto& field : type.Fields) {
			if (field.Name == name) return &field;
		}
		return nullptr;
	}

	std::string ReadText(const std::filesystem::path& path) {
		std::ifstream file(path);
		Require(file.is_open(), "Expected saved scene file to be readable");
		return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
	}

	void VerifyWorkbenchScene(HE::Scene& scene, const glm::vec3& firstPosition,
		HE::Rendering::MaterialBlendMode firstBlendMode, bool expectCamera) {
		struct ExpectedEntity { const char* Uuid; const char* Name; };
		constexpr std::array expectedEntities{
			ExpectedEntity{"00000000000000000000000000000001", "Entity"},
			ExpectedEntity{"00000000000000000000000000000002", "Entity"},
			ExpectedEntity{"00000000000000000000000000000003", "Entity"},
			ExpectedEntity{"00000000000000000000000000000004", "Entity"},
			ExpectedEntity{"00000000000000000000000000000005", "Entity 1"},
		};
		auto& world = scene.GetWorld();
		Require(scene.GetName() == "Untitled Scene" && world.EntityCount() == expectedEntities.size(),
			"Expected workbench scene name and entity count to be preserved");
		for (const auto& expected : expectedEntities) {
			const auto entity = world.Find(HE::EntityUuid::FromString(expected.Uuid));
			Require(world.IsAlive(entity) && world.Name(entity) == expected.Name,
				"Expected workbench entity UUID and name to be preserved");
			Require(world.Has<HE::TransformComponent>(entity) &&
				world.Has<HE::Rendering::MeshComponent>(entity) &&
				world.Has<HE::Rendering::MaterialComponent>(entity),
				"Expected workbench component TypeNames to remain loadable");
		}

		const auto first = world.Find(HE::EntityUuid::FromString(expectedEntities[0].Uuid));
		const auto& transform = ECSTestSupport::Get<HE::TransformComponent>(world, first);
		Require(transform.Position == firstPosition &&
			transform.Scale == glm::vec3(0.5f, 0.499999344f, 0.499999344f),
			"Expected workbench Transform fields to be preserved");
		const auto& mesh = ECSTestSupport::Get<HE::Rendering::MeshComponent>(world, first);
		const auto& material = ECSTestSupport::Get<HE::Rendering::MaterialComponent>(world, first);
		Require(mesh.Mesh.Reference.Guid == HE::BuiltinAssetGuids::SphereMesh &&
			material.Material.Reference.Guid == HE::BuiltinAssetGuids::DefaultMaterial &&
			material.BlendMode == firstBlendMode,
			"Expected workbench built-in asset references and BlendMode to be preserved");
		const auto color = material.Overrides.Parameters.find("u_Color");
		Require(color != material.Overrides.Parameters.end() &&
			std::holds_alternative<glm::vec4>(color->second) &&
			std::get<glm::vec4>(color->second) == glm::vec4(0.800000012f, 0.0f, 0.899999976f, 1.0f),
			"Expected workbench material override values to be preserved");

		const auto fourth = world.Find(HE::EntityUuid::FromString(expectedEntities[3].Uuid));
		const auto& customMesh = ECSTestSupport::Get<HE::Rendering::MeshComponent>(world, fourth);
		const auto& customMaterial = ECSTestSupport::Get<HE::Rendering::MaterialComponent>(world, fourth);
		Require(customMesh.Mesh.Reference.Guid == "15da0d336597b40d17f6cbf870ece1ff" &&
			customMaterial.Material.Reference.Guid == "6de06c0940c1fcd1aa64972a6eaf9f1b",
			"Expected workbench custom asset references to be preserved");
		const auto texture = customMaterial.Overrides.TextureParameters.find("u_Texture");
		Require(texture != customMaterial.Overrides.TextureParameters.end() &&
			texture->second == "dcd87827f9dc6c970784781bb5edc3dc",
			"Expected workbench texture override to be preserved");

		Require(world.Has<HE::Rendering::CameraComponent>(first) == expectCamera,
			"Expected Camera component presence to match the edited scene");
		if (expectCamera) {
			const auto& camera = ECSTestSupport::Get<HE::Rendering::CameraComponent>(world, first);
			Require(camera.Primary && camera.VerticalFovDegrees == 61.0f && camera.FarClip == 250.0f,
				"Expected Camera fields to round-trip between Contexts");
		}
	}

	void VerifyWorkbenchAcrossContexts() {
		const auto fixture = std::filesystem::path(HUAENGINE_TEST_SOURCE_ROOT) /
			"Tests" / "TestProj" / "Assets" / "editor_workbench.scene";
		Require(std::filesystem::is_regular_file(fixture), "Expected read-only workbench scene fixture");
		const auto firstOutput = std::filesystem::temp_directory_path() / "ecs-workbench-context-a.scene";
		const auto secondOutput = std::filesystem::temp_directory_path() / "ecs-workbench-context-b.scene";
		HE::Ecs::EcsContext firstContext;
		HE::Ecs::EcsContext secondContext;
		HE::Scene firstScene(firstContext);
		HE::Scene secondScene(secondContext);
		Require(HE::Serialization::LoadScene(fixture.string(), firstScene),
			"Expected workbench fixture to load in Context A");
		VerifyWorkbenchScene(firstScene, {1.5f, 0.466652006f, -3.35205245f},
			HE::Rendering::MaterialBlendMode::Opaque, false);
		Require(HE::Serialization::SaveScene(firstScene, firstOutput.string()),
			"Expected Context A to save the workbench fixture");
		Require(ReadText(firstOutput).find("version: 3") != std::string::npos,
			"Expected Context A to preserve scene format version 3");
		Require(HE::Serialization::LoadScene(firstOutput.string(), secondScene),
			"Expected Context B to load Context A's saved scene");

		const auto* firstTransform = firstContext.Types().Find<HE::TransformComponent>();
		const auto* firstMaterial = firstContext.Types().Find<HE::Rendering::MaterialComponent>();
		const auto* secondTransform = secondScene.GetWorld().Types().Find<HE::TransformComponent>();
		const auto* secondMaterial = secondScene.GetWorld().Types().Find<HE::Rendering::MaterialComponent>();
		const auto* firstBlendEnum = firstContext.Types().FindEnum("HE::Rendering::MaterialBlendMode");
		const auto* secondBlendEnum = secondContext.Types().FindEnum("HE::Rendering::MaterialBlendMode");
		Require(firstTransform && firstMaterial && secondTransform && secondMaterial &&
			firstBlendEnum && secondBlendEnum && firstTransform->Descriptor.Reflection &&
			firstMaterial->Descriptor.Reflection && secondTransform->Descriptor.Reflection &&
			secondMaterial->Descriptor.Reflection,
			"Expected reflected components and enum in both Contexts");
		Require(firstTransform->Descriptor.Reflection != secondTransform->Descriptor.Reflection &&
			firstMaterial->Descriptor.Reflection != secondMaterial->Descriptor.Reflection &&
			firstBlendEnum != secondBlendEnum,
			"Expected each Context to own its reflection and enum descriptors");
		const auto* positionField = FindField(*secondTransform->Descriptor.Reflection, "Position");
		const auto* blendField = FindField(*secondMaterial->Descriptor.Reflection, "BlendMode");
		Require(positionField && blendField && HE::Refl::IsRuntimeFieldEditable(*positionField) &&
			HE::Refl::IsRuntimeFieldEditable(*blendField) && blendField->EnumType == secondBlendEnum,
			"Expected Context B's editable fields to bind its own enum");
		const auto firstId = HE::EntityUuid::FromString("00000000000000000000000000000001");
		{
			auto edit = ECSTestSupport::Take(HE::Ecs::WorldEditScope::Acquire(secondScene.GetWorld()));
			auto& world = edit.Get();
			const auto entity = world.Find(firstId);
			Require(world.IsAlive(entity) && !world.Has<HE::Rendering::CameraComponent>(entity),
				"Expected the workbench fixture to lack Camera before editing");
			ECSTestSupport::Check(world.Emplace<HE::Rendering::CameraComponent>(entity));
			auto* camera = world.TryGet<HE::Rendering::CameraComponent>(entity);
			Require(camera != nullptr, "Expected Camera insertion in Context B");
			camera->VerticalFovDegrees = 61.0f;
			camera->FarClip = 250.0f;
			ECSTestSupport::Check(world.PublishWrite(entity,
				secondContext.Types().Find<HE::Rendering::CameraComponent>()->Id));
			void* transform = world.TryGet(entity, secondTransform->Id);
			void* material = world.TryGet(entity, secondMaterial->Id);
			Require(transform && material &&
				HE::Refl::SetRuntimeFieldValue(*positionField, transform, glm::vec3(7.0f, 8.0f, 9.0f)) &&
				HE::Refl::SetRuntimeEnumFieldValueByName(*blendField, material, "Transparent"),
				"Expected Context B's fields to edit Transform and BlendMode");
			ECSTestSupport::Check(world.PublishWrite(entity, secondTransform->Id));
			ECSTestSupport::Check(world.PublishWrite(entity, secondMaterial->Id));
		}
		VerifyWorkbenchScene(secondScene, {7.0f, 8.0f, 9.0f},
			HE::Rendering::MaterialBlendMode::Transparent, true);
		Require(HE::Serialization::SaveScene(secondScene, secondOutput.string()),
			"Expected Context B to save its reflected edits");
		const auto savedText = ReadText(secondOutput);
		Require(savedText.find("version: 3") != std::string::npos &&
			savedText.find("TransformComponent:") != std::string::npos &&
			savedText.find("CameraComponent:") != std::string::npos &&
			savedText.find("MeshComponent:") != std::string::npos &&
			savedText.find("MaterialComponent:") != std::string::npos &&
			savedText.find("Position:") != std::string::npos &&
			savedText.find("BlendMode: Transparent") != std::string::npos,
			"Expected stable version 3 component and field names in Context B's scene");
		Require(HE::Serialization::LoadScene(secondOutput.string(), firstScene),
			"Expected Context A to reload Context B's edited scene");
		VerifyWorkbenchScene(firstScene, {7.0f, 8.0f, 9.0f},
			HE::Rendering::MaterialBlendMode::Transparent, true);
		Require(firstContext.Types().Find<HE::TransformComponent>()->Descriptor.Reflection ==
			firstTransform->Descriptor.Reflection &&
			firstContext.Types().FindEnum("HE::Rendering::MaterialBlendMode") == firstBlendEnum,
			"Expected Context A to retain its own reflection after reloading");
		std::error_code error;
		std::filesystem::remove(firstOutput, error);
		Require(!error, "Expected Context A's temporary scene to be removed");
		std::filesystem::remove(secondOutput, error);
		Require(!error, "Expected Context B's temporary scene to be removed");
	}
}

int main() {
	HE::Serialization::InitializeSerialization();
	std::error_code removeError;

	HE::Scene scene("Serialized ECS Scene");
	auto entity = ECSTestSupport::Take(scene.CreateEntity("Camera"));
	auto& transform = ECSTestSupport::Add<HE::TransformComponent>(scene.GetWorld(), entity);
	transform.Position.x = 3.0f;
	transform.Position.y = 4.0f;
	transform.Rotation = { 10.0f, 20.0f, 30.0f };
	transform.Scale = { 2.0f, 3.0f, 4.0f };
	auto& mesh = ECSTestSupport::Add<HE::Rendering::MeshComponent>(scene.GetWorld(), entity);
	mesh.Mesh.Reference.Guid = HE::BuiltinAssetGuids::QuadMesh;
	auto& material = ECSTestSupport::Add<HE::Rendering::MaterialComponent>(scene.GetWorld(), entity);
	material.Material.Reference.Guid = HE::BuiltinAssetGuids::DefaultMaterial;
	material.Overrides.SetVec4("u_BaseColor", glm::vec4(1.0f, 0.0f, 1.0f, 1.0f));

	const auto uuid = scene.GetWorld().Uuid(entity);
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
	Require(loaded.GetWorld().EntityCount() == 1, "Expected one scene entity to round-trip");

	auto loadedEntity = loaded.GetWorld().Find(uuid);
	Require(loaded.GetWorld().IsAlive(loadedEntity), "Expected scene entity uuid to round-trip");
	Require(loaded.GetWorld().Name(loadedEntity) == "Camera", "Expected scene entity name to round-trip");
	Require(loaded.GetWorld().Has<HE::TransformComponent>(loadedEntity), "Expected scene entity transform to round-trip");
	Require(loaded.GetWorld().Has<HE::Rendering::MeshComponent>(loadedEntity), "Expected scene entity mesh to round-trip");
	Require(loaded.GetWorld().Has<HE::Rendering::MaterialComponent>(loadedEntity), "Expected scene entity material to round-trip");
	const auto& loadedTransform = ECSTestSupport::Get<HE::TransformComponent>(loaded.GetWorld(), loadedEntity);
	Require(loadedTransform.Position.x == 3.0f, "Expected Position.x to round-trip");
	Require(loadedTransform.Position.y == 4.0f, "Expected Position.y to round-trip");
	Require(loadedTransform.Rotation == glm::vec3(10.0f, 20.0f, 30.0f), "Expected Rotation to round-trip");
	Require(loadedTransform.Scale == glm::vec3(2.0f, 3.0f, 4.0f), "Expected Scale to round-trip");
	const auto& loadedMesh = ECSTestSupport::Get<HE::Rendering::MeshComponent>(loaded.GetWorld(), loadedEntity);
	Require(loadedMesh.Mesh.Reference.Guid == HE::BuiltinAssetGuids::QuadMesh, "Expected Mesh GUID to round-trip");
	const auto& loadedMaterial = ECSTestSupport::Get<HE::Rendering::MaterialComponent>(loaded.GetWorld(), loadedEntity);
	Require(loadedMaterial.Material.Reference.Guid == HE::BuiltinAssetGuids::DefaultMaterial, "Expected Material GUID to round-trip");
	const auto overrideIt = loadedMaterial.Overrides.Parameters.find("u_BaseColor");
	Require(overrideIt != loadedMaterial.Overrides.Parameters.end(), "Expected vec4 material override to round-trip");
	Require(std::holds_alternative<glm::vec4>(overrideIt->second), "Expected u_BaseColor override to be vec4");
	Require(std::get<glm::vec4>(overrideIt->second) == glm::vec4(1.0f, 0.0f, 1.0f, 1.0f), "Expected vec4 override value to round-trip");

	std::filesystem::remove(path, removeError);
	VerifyWorkbenchAcrossContexts();
	return 0;
}
