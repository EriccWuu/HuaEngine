#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "HuaEngine/ECS/ComponentRegistry.h"
#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/Generated/GeneratedReflection.h"
#include "HuaEngine/Reflection/Reflection.h"
#include "Module/Rendering/RenderingComponent.h"
#include "Panels/RuntimeInspector.h"

namespace {
	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[EditorInspectorRuntimeSmoke] " << message << std::endl;
			std::exit(1);
		}
	}

	const HE::Refl::RuntimeFieldDescriptor* FindField(
		std::span<const HE::Refl::RuntimeFieldDescriptor> fields,
		std::string_view name) {
		for (const auto& field : fields) {
			if (field.Name == name) {
				return &field;
			}
		}
		return nullptr;
	}
}

int main() {
	HE::ComponentRegistry registry;
	HE::RegisterCoreComponents(registry);

	const HE::ComponentMetadata* transform = registry.FindByType<HE::TransformComponent>();
	Require(transform != nullptr, "Expected TransformComponent metadata");
	Require(transform->RuntimeType != nullptr, "Expected TransformComponent runtime type");
	const auto* position = FindField(transform->RuntimeType->Fields, "Position");
	Require(position != nullptr, "Expected TransformComponent Position field");
	Require(
		HE::Editor::GetRuntimeFieldEditKind(*position) == HE::Editor::RuntimeFieldEditKind::Float3,
		"Expected Position to use Float3 runtime editor");

	const HE::ComponentMetadata* camera = registry.FindByType<HE::Rendering::CameraComponent>();
	Require(camera != nullptr, "Expected CameraComponent metadata");
	const auto* primary = FindField(camera->RuntimeType->Fields, "Primary");
	Require(primary != nullptr, "Expected CameraComponent Primary field");
	Require(
		HE::Editor::GetRuntimeFieldEditKind(*primary) == HE::Editor::RuntimeFieldEditKind::Bool,
		"Expected CameraComponent Primary to use Bool runtime editor");

	const HE::ComponentMetadata* mesh = registry.FindByType<HE::Rendering::MeshComponent>();
	Require(mesh != nullptr, "Expected MeshComponent metadata");
	const auto* meshAssetName = FindField(mesh->RuntimeType->Fields, "MeshAssetName");
	Require(meshAssetName != nullptr, "Expected MeshComponent MeshAssetName field");
	Require(
		HE::Editor::GetRuntimeFieldEditKind(*meshAssetName) == HE::Editor::RuntimeFieldEditKind::String,
		"Expected MeshAssetName to use String runtime editor");

	const HE::ComponentMetadata* material = registry.FindByType<HE::Rendering::MaterialComponent>();
	Require(material != nullptr, "Expected MaterialComponent metadata");
	const auto* materialInstance = FindField(material->RuntimeType->Fields, "MaterialInstance");
	Require(materialInstance != nullptr, "Expected MaterialComponent MaterialInstance field");
	Require(
		HE::Editor::GetRuntimeFieldEditKind(*materialInstance) == HE::Editor::RuntimeFieldEditKind::Unsupported,
		"Expected MaterialInstance to use unsupported runtime editor");

	std::cout << "EditorInspectorRuntimeSmoke passed" << std::endl;
	return 0;
}
