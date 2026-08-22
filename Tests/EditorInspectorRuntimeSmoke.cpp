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
		HE::Refl::GetRuntimeFieldValueKind(*position) == HE::Refl::RuntimeFieldValueKind::Float3,
		"Expected Position to use Float3 runtime editor");
	Require(HE::Editor::IsRuntimeFieldEditable(*position), "Expected Position to be runtime editable");

	const HE::ComponentMetadata* camera = registry.FindByType<HE::Rendering::CameraComponent>();
	Require(camera != nullptr, "Expected CameraComponent metadata");
	const auto* primary = FindField(camera->RuntimeType->Fields, "Primary");
	Require(primary != nullptr, "Expected CameraComponent Primary field");
	Require(
		HE::Refl::GetRuntimeFieldValueKind(*primary) == HE::Refl::RuntimeFieldValueKind::Bool,
		"Expected CameraComponent Primary to use Bool runtime editor");

	const HE::ComponentMetadata* mesh = registry.FindByType<HE::Rendering::MeshComponent>();
	Require(mesh != nullptr, "Expected MeshComponent metadata");
	const auto* meshAsset = FindField(mesh->RuntimeType->Fields, "Mesh");
	Require(meshAsset != nullptr, "Expected MeshComponent Mesh field");
	Require(
		HE::Refl::GetRuntimeFieldValueKind(*meshAsset) == HE::Refl::RuntimeFieldValueKind::AssetRef,
		"Expected Mesh to use AssetRef runtime editor");
	Require(HE::Editor::IsRuntimeFieldEditable(*meshAsset), "Expected Mesh asset ref to be runtime editable");
	Require(FindField(mesh->RuntimeType->Fields, "MeshAssetName") == nullptr, "Expected MeshAssetName field to be removed");

	const HE::ComponentMetadata* material = registry.FindByType<HE::Rendering::MaterialComponent>();
	Require(material != nullptr, "Expected MaterialComponent metadata");
	const auto* materialAsset = FindField(material->RuntimeType->Fields, "Material");
	Require(materialAsset != nullptr, "Expected MaterialComponent Material field");
	Require(
		HE::Refl::GetRuntimeFieldValueKind(*materialAsset) == HE::Refl::RuntimeFieldValueKind::AssetRef,
		"Expected Material to use AssetRef runtime field kind");
	Require(HE::Editor::IsRuntimeFieldEditable(*materialAsset), "Expected Material asset ref to be runtime editable");
	const auto* overrides = FindField(material->RuntimeType->Fields, "Overrides");
	Require(overrides != nullptr, "Expected MaterialComponent Overrides field");
	Require(
		HE::Refl::GetRuntimeFieldValueKind(*overrides) == HE::Refl::RuntimeFieldValueKind::Object,
		"Expected Overrides to use Object runtime field kind");
	Require(FindField(material->RuntimeType->Fields, "MaterialInstance") == nullptr, "Expected MaterialInstance field to be removed");
	const auto* blendMode = FindField(material->RuntimeType->Fields, "BlendMode");
	Require(blendMode != nullptr, "Expected MaterialComponent BlendMode field");
	Require(
		HE::Refl::GetRuntimeFieldValueKind(*blendMode) == HE::Refl::RuntimeFieldValueKind::Enum,
		"Expected BlendMode to use Enum runtime field kind");
	Require(HE::Editor::IsRuntimeFieldEditable(*blendMode), "Expected BlendMode to be runtime editable");
	Require(blendMode->EnumType != nullptr, "Expected BlendMode enum metadata");

	Require(
		HE::Editor::GetRuntimeComponentDisplayName(*transform->RuntimeType) == "Transform",
		"Expected Transform display name from runtime metadata");
	Require(
		HE::Editor::GetRuntimeComponentDisplayName(*camera->RuntimeType) == "Camera",
		"Expected Camera display name from runtime metadata");
	Require(
		registry.FindByName("NameComponent") == nullptr,
		"Expected entity names to stay out of runtime component candidates");
	Require(
		registry.FindByName("RendererComponent") == nullptr,
		"Expected deprecated RendererComponent to stay out of generated runtime metadata");

	std::filesystem::path repositoryRoot = std::filesystem::current_path();
	while (!repositoryRoot.empty() && !std::filesystem::exists(repositoryRoot / "CMakeLists.txt")) {
		repositoryRoot = repositoryRoot.parent_path();
	}
	Require(!repositoryRoot.empty(), "Expected to locate repository root");

	const std::filesystem::path inspectorPath = repositoryRoot / "Editor" / "src" / "Panels" / "InspectorPanel.cpp";
	std::ifstream inspectorStream(inspectorPath);
	Require(inspectorStream.good(), "Expected InspectorPanel.cpp to be readable");
	std::stringstream inspectorBuffer;
	inspectorBuffer << inspectorStream.rdbuf();
	const std::string inspectorSource = inspectorBuffer.str();
	const std::filesystem::path runtimeInspectorPath = repositoryRoot / "Editor" / "src" / "Panels" / "RuntimeInspector.cpp";
	std::ifstream runtimeInspectorStream(runtimeInspectorPath);
	Require(runtimeInspectorStream.good(), "Expected RuntimeInspector.cpp to be readable");
	std::stringstream runtimeInspectorBuffer;
	runtimeInspectorBuffer << runtimeInspectorStream.rdbuf();
	const std::string runtimeInspectorSource = runtimeInspectorBuffer.str();
	Require(
		runtimeInspectorSource.find("DrawRuntimeAssetRefField") != std::string::npos,
		"Expected RuntimeInspector to draw typed asset refs explicitly");
	Require(
		runtimeInspectorSource.find("DrawAssetRefField") != std::string::npos &&
			runtimeInspectorSource.find("DrawMeshAssetRefField") == std::string::npos,
		"Expected mesh and material refs to share the parameterized asset picker");
	Require(
		runtimeInspectorSource.find("field.Type == \"MeshAssetRef\" || field.Type == \"MaterialAssetRef\"") != std::string::npos,
		"Expected both mesh and material refs to use the shared picker");
	Require(
		runtimeInspectorSource.find("DrawRuntimeFieldEditorRow") != std::string::npos,
		"Expected all reflected fields to use one property-row implementation");
	Require(
		runtimeInspectorSource.find("BeginRuntimeFieldTable(\"##RuntimeFields\")") != std::string::npos,
		"Expected each component to share one aligned field table");
	Require(
		runtimeInspectorSource.find("TableSetColumnIndex(0)") != std::string::npos &&
			runtimeInspectorSource.find("TableSetColumnIndex(1)") != std::string::npos,
		"Expected field labels before values in separate table columns");
	Require(
		runtimeInspectorSource.find("case Refl::RuntimeFieldValueKind::AssetRef") != std::string::npos,
		"Expected RuntimeInspector AssetRef switch case");
	Require(
		inspectorSource.find("ComponentEditorRegistry") == std::string::npos,
		"Expected InspectorPanel not to use legacy ComponentEditorRegistry");
	Require(
		inspectorSource.find("Refl::reflect") == std::string::npos,
		"Expected InspectorPanel not to use static reflection field traversal");
	Require(
		inspectorSource.find("ListComponentTypes") != std::string::npos,
		"Expected InspectorPanel to enumerate entity runtime component types");
	Require(
		inspectorSource.find("PushID(static_cast<int>(selection.GetUid()))") != std::string::npos,
		"Expected inspector widget IDs to be scoped by selected entity");
	Require(
		inspectorSource.find("GetAll()") != std::string::npos,
		"Expected Add Component candidates to come from ComponentRegistry::GetAll()");
	Require(
		!std::filesystem::exists(repositoryRoot / "Editor" / "src" / "ComponentEditorRegistry.h"),
		"Expected legacy ComponentEditorRegistry.h to be removed");
	Require(
		!std::filesystem::exists(repositoryRoot / "Editor" / "src" / "ComponentEditor.h"),
		"Expected legacy ComponentEditor.h to be removed");

	std::cout << "EditorInspectorRuntimeSmoke passed" << std::endl;
	return 0;
}
