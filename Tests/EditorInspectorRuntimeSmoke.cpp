#include <cstdlib>
#include <array>
#include <iostream>
#include <string>
#include <vector>

#include "HuaEngine/ECS/Runtime/TypeRegistry.h"
#include "HuaEngine/Asset/AssetReferenceCapability.h"
#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/Generated/GeneratedEcs.h"
#include "HuaEngine/Reflection/Reflection.h"
#include "Module/Rendering/RenderingComponent.h"
#include "Panels/RuntimeInspector.h"
#include "imgui.h"

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
	HE::Rendering::MaterialParameterDefinition rangedParameter;
	rangedParameter.Range = { -2.0f, 3.0f };
	rangedParameter.Step = 0.25f;
	const auto rangedOptions = HE::Editor::GetMaterialNumericEditorOptions(rangedParameter);
	Require(rangedOptions.HasRange, "Expected two-value material range to enable numeric clamping");
	Require(rangedOptions.Minimum == -2.0f && rangedOptions.Maximum == 3.0f, "Expected material range bounds to reach the editor widget");
	Require(rangedOptions.Speed == 0.25f, "Expected material step to control editor drag speed");

	HE::Rendering::MaterialParameterDefinition invalidRangeParameter;
	invalidRangeParameter.Range = { 3.0f, -2.0f };
	const auto invalidRangeOptions = HE::Editor::GetMaterialNumericEditorOptions(invalidRangeParameter);
	Require(!invalidRangeOptions.HasRange, "Expected reversed material range not to clamp the editor widget");
	Require(invalidRangeOptions.Speed == 0.1f, "Expected default material drag speed when step is absent");

	HE::Ecs::TypeRegistry registry;
	Require(HE::Generated::RegisterGeneratedComponents(registry).HasValue(), "Expected canonical module registration");

	const HE::Ecs::RegisteredType* transform = registry.Find<HE::TransformComponent>();
	Require(transform != nullptr, "Expected TransformComponent metadata");
	Require(transform->Descriptor.Reflection != nullptr, "Expected TransformComponent runtime type");
	const auto* position = FindField(transform->Descriptor.Reflection->Fields, "Position");
	Require(position != nullptr, "Expected TransformComponent Position field");
	Require(
		HE::Refl::GetRuntimeFieldValueKind(*position) == HE::Refl::RuntimeFieldValueKind::Float3,
		"Expected Position to use Float3 runtime editor");
	Require(HE::Editor::IsRuntimeFieldEditable(*position), "Expected Position to be runtime editable");
	HE::Editor::RuntimeFieldDrawerRegistry drawers;
	Require(drawers.Resolve(*position) != nullptr, "Expected a vector field drawer");

	const HE::Ecs::RegisteredType* camera = registry.Find<HE::Rendering::CameraComponent>();
	Require(camera != nullptr, "Expected CameraComponent metadata");
	const auto* primary = FindField(camera->Descriptor.Reflection->Fields, "Primary");
	Require(primary != nullptr, "Expected CameraComponent Primary field");
	Require(
		HE::Refl::GetRuntimeFieldValueKind(*primary) == HE::Refl::RuntimeFieldValueKind::Bool,
		"Expected CameraComponent Primary to use Bool runtime editor");
	Require(drawers.Resolve(*primary) != nullptr, "Expected a bool field drawer");
	const auto* fov = FindField(camera->Descriptor.Reflection->Fields, "VerticalFovDegrees");
	Require(fov != nullptr && drawers.Resolve(*fov) != nullptr, "Expected a float field drawer");
	auto anotherFloatField = *fov;
	anotherFloatField.Name = "AnotherFloat";
	Require(drawers.Resolve(anotherFloatField) == drawers.Resolve(*fov),
		"Expected the default float drawer to be reused across fields");
	Require(HE::Refl::FindRuntimeAttribute(fov->Attributes, "Editor.Min") != nullptr,
		"Expected float range metadata to reach the field drawer");

	const HE::Ecs::RegisteredType* mesh = registry.Find<HE::Rendering::MeshComponent>();
	Require(mesh != nullptr, "Expected MeshComponent metadata");
	const auto* meshAsset = FindField(mesh->Descriptor.Reflection->Fields, "Mesh");
	Require(meshAsset != nullptr, "Expected MeshComponent Mesh field");
	Require(meshAsset->ValueTypeGuid == HE::AssetReferenceTypeGuids::Mesh,
		"Expected Mesh field to retain its value type Guid");
	Require(HE::Editor::IsRuntimeFieldEditable(*meshAsset), "Expected Mesh asset ref to be runtime editable");
	Require(drawers.Resolve(*meshAsset) != nullptr, "Expected a mesh asset field drawer");
	const auto* meshCapability = HE::FindAssetReferenceCapability(meshAsset->ValueTypeGuid);
	Require(meshCapability != nullptr && meshCapability->Kind == HE::AssetKind::Mesh,
		"Expected Mesh asset kind to come from its value type capability");
	const auto* reflectedMeshRef = registry.Reflection().FindType(meshAsset->ValueTypeGuid);
	Require(reflectedMeshRef != nullptr && reflectedMeshRef->Guid == HE::AssetReferenceTypeGuids::Mesh,
		"Expected asset value type Guid to resolve in reflection");
	Require(FindField(mesh->Descriptor.Reflection->Fields, "MeshAssetName") == nullptr, "Expected MeshAssetName field to be removed");

	const HE::Ecs::RegisteredType* material = registry.Find<HE::Rendering::MaterialComponent>();
	Require(material != nullptr, "Expected MaterialComponent metadata");
	const auto* materialAsset = FindField(material->Descriptor.Reflection->Fields, "Material");
	Require(materialAsset != nullptr, "Expected MaterialComponent Material field");
	Require(materialAsset->ValueTypeGuid == HE::AssetReferenceTypeGuids::Material,
		"Expected Material field to retain its value type Guid");
	Require(HE::Editor::IsRuntimeFieldEditable(*materialAsset), "Expected Material asset ref to be runtime editable");
	Require(drawers.Resolve(*materialAsset) == drawers.Resolve(*meshAsset),
		"Expected material and mesh references to share the asset field drawer");
	Require(HE::FindAssetReferenceCapability(materialAsset->ValueTypeGuid)->Kind == HE::AssetKind::Material,
		"Expected Material asset kind from the value type capability");
	auto misleadingAssetField = *meshAsset;
	misleadingAssetField.Type = "ShaderAssetRef";
	Require(drawers.Resolve(misleadingAssetField) == drawers.Resolve(*meshAsset),
		"Expected asset drawer selection to use value type Guid rather than type spelling");
	misleadingAssetField.ValueTypeGuid = {};
	Require(drawers.Resolve(misleadingAssetField) == nullptr &&
		!HE::Editor::IsRuntimeFieldEditable(misleadingAssetField),
		"Expected an unregistered value type to have no asset drawer");
	const auto* reconcileField = HE::Refl::FindRuntimeAttribute(materialAsset->Attributes, "Editor.ReconcileField");
	Require(reconcileField != nullptr && reconcileField->Value == "Overrides",
		"Expected material reconciliation to name its sibling field");
	const auto* overrides = FindField(material->Descriptor.Reflection->Fields, "Overrides");
	Require(overrides != nullptr, "Expected MaterialComponent Overrides field");
	Require(
		HE::Refl::GetRuntimeFieldValueKind(*overrides) == HE::Refl::RuntimeFieldValueKind::Object,
		"Expected Overrides to use Object runtime field kind");
	const auto* overridesDrawer = HE::Refl::FindRuntimeAttribute(overrides->Attributes, "Inspector.Drawer");
	Require(overridesDrawer != nullptr && overridesDrawer->Value == "MaterialOverrides",
		"Expected overrides to select a field drawer");
	Require(drawers.Resolve(*overrides) != nullptr, "Expected the material overrides field drawer to be registered");
	const auto* sourceField = HE::Refl::FindRuntimeAttribute(overrides->Attributes, "Editor.SourceField");
	Require(sourceField != nullptr && sourceField->Value == "Material",
		"Expected material parameters to name their source field");
	Require(FindField(material->Descriptor.Reflection->Fields, "MaterialInstance") == nullptr, "Expected MaterialInstance field to be removed");
	const auto* blendMode = FindField(material->Descriptor.Reflection->Fields, "BlendMode");
	Require(blendMode != nullptr, "Expected MaterialComponent BlendMode field");
	Require(
		HE::Refl::GetRuntimeFieldValueKind(*blendMode) == HE::Refl::RuntimeFieldValueKind::Enum,
		"Expected BlendMode to use Enum runtime field kind");
	Require(HE::Editor::IsRuntimeFieldEditable(*blendMode), "Expected BlendMode to be runtime editable");
	Require(blendMode->EnumType != nullptr, "Expected BlendMode enum metadata");
	Require(drawers.Resolve(*blendMode) != nullptr, "Expected an enum field drawer");
	HE::MeshAssetRef meshReference;
	Require(meshCapability->WriteGuid(&meshReference, "mesh-guid") &&
		*meshCapability->ReadGuid(&meshReference) == "mesh-guid",
		"Expected asset reference capability to read and write Mesh Guid");
	HE::AssetReference untypedReference;
	const auto* baseCapability = HE::FindAssetReferenceCapability(HE::AssetReferenceTypeGuids::Base);
	Require(baseCapability != nullptr && baseCapability->Kind == HE::AssetKind::Unknown &&
		baseCapability->WriteGuid(&untypedReference, "untyped-guid") &&
		*baseCapability->ReadGuid(&untypedReference) == "untyped-guid",
		"Expected untyped asset references to expose Guid access");
	HE::MaterialAssetRef materialReference;
	const auto* materialCapability = HE::FindAssetReferenceCapability(HE::AssetReferenceTypeGuids::Material);
	Require(materialCapability != nullptr && materialCapability->Kind == HE::AssetKind::Material &&
		materialCapability->WriteGuid(&materialReference, "material-guid") &&
		*materialCapability->ReadGuid(&materialReference) == "material-guid",
		"Expected asset reference capability to read and write Material Guid");
	HE::TextureAssetRef textureReference;
	const auto* textureCapability = HE::FindAssetReferenceCapability(HE::AssetReferenceTypeGuids::Texture);
	Require(textureCapability != nullptr && textureCapability->Kind == HE::AssetKind::Texture2D &&
		textureCapability->WriteGuid(&textureReference, "texture-guid") &&
		*textureCapability->ReadGuid(&textureReference) == "texture-guid",
		"Expected asset reference capability to read and write Texture Guid");
	HE::ShaderAssetRef shaderReference;
	const auto* shaderCapability = HE::FindAssetReferenceCapability(HE::AssetReferenceTypeGuids::Shader);
	Require(shaderCapability != nullptr && shaderCapability->Kind == HE::AssetKind::Shader &&
		shaderCapability->WriteGuid(&shaderReference, "shader-guid") &&
		*shaderCapability->ReadGuid(&shaderReference) == "shader-guid",
		"Expected asset reference capability to read and write Shader Guid");
	Require(registry.Reflection().FindType(HE::AssetReferenceTypeGuids::Shader) != nullptr,
		"Expected Shader reference type to be reflected");
	for (const auto valueTypeGuid : {HE::AssetReferenceTypeGuids::Mesh,
		HE::AssetReferenceTypeGuids::Material, HE::AssetReferenceTypeGuids::Texture,
		HE::AssetReferenceTypeGuids::Shader}) {
		const auto* reflectedType = registry.Reflection().FindType(valueTypeGuid);
		const auto* capability = HE::FindAssetReferenceCapability(valueTypeGuid);
		Require(reflectedType != nullptr && capability != nullptr && capability->Kind != HE::AssetKind::Unknown,
			"Expected each reflected asset reference type to have an asset access capability");
	}
	Require(HE::FindAssetReferenceCapability({}) == nullptr && !shaderCapability->WriteGuid(nullptr, "shader-guid") &&
		shaderCapability->ReadGuid(nullptr) == nullptr,
		"Expected unknown type and null values to reject asset reference access");

	const std::array<HE::Refl::RuntimeAttribute, 1> customAttributes = {{{"Inspector.Drawer", "Test.Float"}}};
	auto customField = *fov;
	customField.Attributes = customAttributes;
	bool customCalled = false;
	drawers.RegisterNamed("Test.Float", [&](const auto&, const auto&, void*, void*, auto&) {
		customCalled = true;
		return true;
	});
	const auto* customDrawer = drawers.Resolve(customField);
	Require(customDrawer != nullptr && customDrawer != drawers.Resolve(*fov),
		"Expected a named field drawer to take priority over its value kind");
	HE::Editor::RuntimeInspectorContext customContext;
	Require((*customDrawer)(customField, *camera->Descriptor.Reflection, nullptr, nullptr, customContext) && customCalled,
		"Expected the selected custom field drawer to be callable");
	const std::array<HE::Refl::RuntimeAttribute, 1> unknownAttributes = {{{"Inspector.Drawer", "Missing.Drawer"}}};
	auto unknownField = *fov;
	unknownField.Attributes = unknownAttributes;
	Require(drawers.Resolve(unknownField) == nullptr,
		"Expected an unknown named drawer to block fallback to a float drawer");
	auto readOnlyField = *fov;
	readOnlyField.Flags = readOnlyField.Flags | HE::Refl::RuntimeFieldFlags::ReadOnly;
	Require(!HE::Editor::IsRuntimeFieldEditable(readOnlyField), "Expected read-only metadata to block field editing");
	auto unsupportedField = *fov;
	unsupportedField.Type = "UnregisteredFieldType";
	Require(drawers.Resolve(unsupportedField) == nullptr,
		"Expected an unsupported value type to have no default drawer");
	HE::Rendering::CameraComponent cameraCandidate;
	auto singleFieldType = *camera->Descriptor.Reflection;
	singleFieldType.Fields = std::span<const HE::Refl::RuntimeFieldDescriptor>(&customField, 1);
	ImGui::CreateContext();
	ImGui::GetIO().DisplaySize = ImVec2(800.0f, 600.0f);
	ImGui::GetIO().DeltaTime = 1.0f / 60.0f;
	ImGui::GetIO().Fonts->Build();
	ImGui::NewFrame();
	ImGui::Begin("Field drawer smoke");
	customCalled = false;
	Require(HE::Editor::DrawRuntimeFields(singleFieldType, &cameraCandidate, drawers, customContext) && customCalled,
		"Expected reflected field traversal to invoke the selected drawer");
	singleFieldType.Fields = std::span<const HE::Refl::RuntimeFieldDescriptor>(&unknownField, 1);
	Require(!HE::Editor::DrawRuntimeFields(singleFieldType, &cameraCandidate, drawers, customContext),
		"Expected an unknown named drawer to leave the candidate unchanged");
	ImGui::End();
	ImGui::Render();
	ImGui::DestroyContext();

	HE::Rendering::MaterialComponent candidate;
	candidate.Material.Reference.Guid = "previous-material";
	candidate.Overrides.SetFloat("Compatible", 2.5f);
	candidate.Overrides.SetFloat("Obsolete", 1.0f);
	HE::Rendering::MaterialParameterDefinition compatibleParameter;
	compatibleParameter.Name = "Compatible";
	compatibleParameter.Type = HE::Rendering::ShaderValueType::Float;
	const HE::Rendering::MaterialDefinition nextDefinition({compatibleParameter}, {});
	std::vector<HE::ResultEnvelope> deferredEvents;
	HE::Editor::RuntimeInspectorContext materialContext;
	materialContext.ResolveMaterialDefinition = [&](const HE::AssetGuid& guid,
		HE::Rendering::MaterialDefinition& definition, HE::AssetImportHealth&) {
		Require(guid == "next-material", "Expected the next material GUID to be resolved");
		definition = nextDefinition;
		return HE::ResultEnvelope::Success("test.resolve_material", guid);
	};
	materialContext.DeferredEvents = &deferredEvents;
	Require(HE::Editor::SetRuntimeMaterialReference(*materialAsset, *material->Descriptor.Reflection,
		&candidate, "next-material", materialContext), "Expected material reference editing to change the candidate");
	Require(candidate.Material.Reference.Guid == "next-material", "Expected the candidate material GUID to change");
	Require(candidate.Overrides.Parameters.contains("Compatible") &&
		!candidate.Overrides.Parameters.contains("Obsolete"),
		"Expected material reconciliation to preserve compatible overrides only");
	Require(deferredEvents.size() == 1 && !deferredEvents.front().Details.empty(),
		"Expected an override warning to remain deferred until command commit");
	Require(!HE::Editor::SetRuntimeMaterialReference(*materialAsset, *material->Descriptor.Reflection,
		&candidate, "next-material", materialContext) && deferredEvents.size() == 1,
		"Expected selecting the current material to leave the candidate unchanged");
	Require(!HE::Editor::SetRuntimeMaterialReference(*materialAsset, *material->Descriptor.Reflection,
		nullptr, "another-material", materialContext), "Expected a null candidate to be rejected safely");
	const std::array<HE::Refl::RuntimeAttribute, 1> missingSibling = {{{"Editor.ReconcileField", "Missing"}}};
	auto invalidMaterialField = *materialAsset;
	invalidMaterialField.Attributes = missingSibling;
	Require(!HE::Editor::SetRuntimeMaterialReference(invalidMaterialField, *material->Descriptor.Reflection,
		&candidate, "another-material", materialContext), "Expected a missing sibling field to prevent editing");
	Require(candidate.Material.Reference.Guid == "next-material", "Expected invalid metadata to preserve the candidate");

	Require(
		HE::Editor::GetRuntimeComponentDisplayName(*transform->Descriptor.Reflection) == "Transform",
		"Expected Transform display name from runtime metadata");
	Require(
		HE::Editor::GetRuntimeComponentDisplayName(*camera->Descriptor.Reflection) == "Camera",
		"Expected Camera display name from runtime metadata");
	Require(
		registry.FindByName("NameComponent") == nullptr,
		"Expected entity names to stay out of runtime component candidates");
	Require(
		registry.FindByName("RendererComponent") == nullptr,
		"Expected deprecated RendererComponent to stay out of generated runtime metadata");

	std::cout << "EditorInspectorRuntimeSmoke passed" << std::endl;
	return 0;
}
