#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include "Assets/AssetEditorRegistry.h"
#include "Assets/AssetInspectorHost.h"
#include "Assets/Editors/GenericAssetInspector.h"
#include "Assets/Editors/MaterialAssetEditor.h"
#include "Assets/Editors/ImportSettingsEditors.h"
#include "Assets/Editors/SceneAssetEditor.h"
#include "HuaEngine/Asset/Import/AssetSourceHash.h"
#include "HuaEngine/Asset/Metadata/AssetMeta.h"

namespace {
	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[AssetEditingSmoke] " << message << std::endl;
			std::exit(1);
		}
	}

	class TestAssetEditor final : public HE::Editor::IAssetEditor {
	public:
		HE::ResultEnvelope Open(const HE::Editor::AssetEditorOpenContext& context) override {
			m_Guid = context.Snapshot.Asset.Guid;
			return HE::ResultEnvelope::Success("asset.editor.test", m_Guid, "Test asset editor opened");
		}
		void Draw(HE::Editor::AssetEditorDrawContext&) override {}
		HE::ResultEnvelope Validate() const override { return HE::ResultEnvelope::Success("asset.editor.test.validate", m_Guid, "Valid"); }
		HE::AssetEditCommit BuildCommit() const override { return { .Guid = m_Guid }; }
		bool IsDirty() const override { return false; }
		void Revert() override {}
	private:
		HE::AssetGuid m_Guid;
	};
}

int main() {
	HE::Editor::AssetEditorRegistry registry;
	registry.SetFallbackFactory([] { return std::make_unique<HE::Editor::GenericAssetInspector>(); });
	Require(registry.Register({ HE::AssetKind::Material, "material.native" }, [] { return std::make_unique<TestAssetEditor>(); }).Succeeded(), "Expected asset editor registration");
	Require(registry.Register({ HE::AssetKind::Material, "material.native" }, [] { return std::make_unique<TestAssetEditor>(); }).Failed(), "Expected duplicate editor rejection");
	Require(dynamic_cast<TestAssetEditor*>(registry.Create(HE::AssetKind::Material, "material.native").get()) != nullptr, "Expected exact editor match");
	Require(dynamic_cast<HE::Editor::GenericAssetInspector*>(registry.Create(HE::AssetKind::Mesh, "mesh.obj").get()) != nullptr, "Expected generic editor fallback");

	HE::Editor::AssetEditSession session;
	HE::AssetInspectionSnapshot snapshot;
	const auto sessionRoot = std::filesystem::temp_directory_path() / "HuaEngineAssetEditingSmoke";
	const auto sourcePath = sessionRoot / "Test.material";
	std::error_code errorCode;
	std::filesystem::remove_all(sessionRoot, errorCode);
	std::filesystem::create_directories(sessionRoot, errorCode);
	{
		std::ofstream stream(sourcePath);
		stream << "name: Test\n";
	}
	Require(HE::SaveAssetMeta(sourcePath, { .Guid = "asset-guid", .ImporterId = "material.native" }).Succeeded(), "Expected session metadata fixture");
	snapshot.Asset = { .Guid = "asset-guid", .Kind = HE::AssetKind::Material, .Source = HE::AssetSource::File, .AssetId = "Test.material", .AbsolutePath = sourcePath, .ExistsOnDisk = true };
	snapshot.ImporterId = "material.native";
	Require(HE::ComputeAssetSourceHash(sourcePath, snapshot.SourceContentHash).Succeeded(), "Expected source baseline hash");
	Require(HE::ComputeAssetSourceHash(HE::GetAssetMetaPath(sourcePath), snapshot.MetaContentHash).Succeeded(), "Expected metadata baseline hash");
	session.Open(snapshot);
	Require(session.IsOpen() && !session.IsDirty(), "Expected clean opened asset edit session");
	Require(session.CheckExternalModification().Succeeded() && !session.IsExternallyModified(), "Expected unchanged edit session baseline");
	{
		std::ofstream stream(sourcePath, std::ios::app);
		stream << "external: true\n";
	}
	Require(session.CheckExternalModification().RequiresManualIntervention() && session.IsExternallyModified(), "Expected external source conflict");
	session.MarkDirty();
	Require(session.IsDirty(), "Expected session dirty state");
	session.MarkClean();
	Require(!session.IsDirty(), "Expected session clean state");
	session.Close();
	Require(!session.IsOpen(), "Expected closed asset edit session");
	HE::Rendering::MaterialSourceData materialSource{ .Name = "Editable", .Type = HE::Rendering::MaterialType::Custom, .ShaderGuid = "shader-guid" };
	materialSource.Parameters.emplace("u_Value", HE::Rendering::MaterialSourceParameter{ "u_Value", HE::Rendering::MaterialParameterType::Float, 0.5f });
	Require(HE::Rendering::SaveMaterialSourceData(sourcePath, materialSource).Succeeded(), "Expected editable material fixture");
	Require(HE::ComputeAssetSourceHash(sourcePath, snapshot.SourceContentHash).Succeeded(), "Expected editable source hash");
	HE::Editor::MaterialAssetEditor materialEditor;
	Require(materialEditor.Open({ snapshot }).Succeeded() && !materialEditor.IsDirty(), "Expected clean material editor working copy");
	materialEditor.GetWorkingCopy().Parameters.at("u_Value").Value = 0.75f;
	Require(materialEditor.IsDirty() && materialEditor.Validate().Succeeded(), "Expected valid dirty material working copy");
	Require(!materialEditor.BuildCommit().SerializedContent.empty(), "Expected serialized material edit commit");
	HE::Rendering::ShaderAuthoringMetadata shaderMetadata;
	shaderMetadata.Parameters.push_back({ .Name = "u_Color", .Scope = HE::Rendering::ShaderParameterScope::Material, .Type = HE::Rendering::ShaderValueType::Float4, .Editor = HE::Rendering::ShaderEditorKind::Color, .DefaultValue = glm::vec4(1.0f) });
	Require(materialEditor.ReconcileShader(shaderMetadata).Succeeded(), "Expected shader parameter reconciliation");
	Require(!materialEditor.GetWorkingCopy().Parameters.contains("u_Value") && materialEditor.GetWorkingCopy().Parameters.contains("u_Color"), "Expected incompatible parameters replaced by shader defaults");
	materialEditor.Revert();
	Require(!materialEditor.IsDirty(), "Expected material editor revert");
	HE::Editor::AssetEditorDrawContext invalidReferenceContext;
	invalidReferenceContext.GetShaderAuthoringMetadata = [](const HE::AssetGuid&, HE::Rendering::ShaderAuthoringMetadata&) { return HE::ResultEnvelope::Failure("asset.shader", "shader-guid", "Unhealthy"); };
	Require(materialEditor.ValidateReferences(invalidReferenceContext).Failed(), "Expected unhealthy shader reference rejection");
	const auto objPath = sessionRoot / "Editable.obj";
	{
		std::ofstream stream(objPath); stream << "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
	}
	Require(HE::SaveAssetMeta(objPath, { .Guid = "obj-guid", .ImporterId = "hua.mesh-obj" }).Succeeded(), "Expected OBJ metadata fixture");
	HE::AssetInspectionSnapshot objSnapshot;
	objSnapshot.Asset = { .Guid = "obj-guid", .Kind = HE::AssetKind::Mesh, .Source = HE::AssetSource::File, .AssetId = "Editable.obj", .AbsolutePath = objPath, .ExistsOnDisk = true };
	objSnapshot.ImporterId = "hua.mesh-obj";
	Require(HE::ComputeAssetSourceHash(objPath, objSnapshot.SourceContentHash).Succeeded() && HE::ComputeAssetSourceHash(HE::GetAssetMetaPath(objPath), objSnapshot.MetaContentHash).Succeeded(), "Expected OBJ editor hashes");
	HE::Editor::ObjMeshImportEditor objEditor;
	Require(objEditor.Open({ objSnapshot }).Succeeded() && !objEditor.IsDirty(), "Expected OBJ settings editor open");
	objEditor.GetWorkingCopy().ImportScale = 2.0f;
	Require(objEditor.IsDirty() && objEditor.Validate().Succeeded() && objEditor.BuildCommit().Target == HE::AssetEditTarget::Metadata, "Expected editable OBJ metadata commit");
	objEditor.Revert();

	HE::Editor::AssetInspectorHost host;
	Require(host.GetRegistry().Register({ HE::AssetKind::Material, "material.native" }, [] { return std::make_unique<TestAssetEditor>(); }).Succeeded(), "Expected host editor registration");
	const auto openResult = host.Open("asset-guid", [snapshot](const HE::AssetGuid& guid, HE::AssetInspectionSnapshot& output) {
		if (guid != snapshot.Asset.Guid) return HE::ResultEnvelope::Failure("asset.inspect", guid, "Unexpected GUID");
		output = snapshot;
		return HE::ResultEnvelope::Success("asset.inspect", guid, "Inspected");
	});
	Require(openResult.Succeeded() && host.GetSession().IsOpen(), "Expected asset inspector host session");
	Require(dynamic_cast<TestAssetEditor*>(host.GetEditor()) != nullptr, "Expected host to select registered editor");
	host.Close();
	Require(!host.GetSession().IsOpen() && host.GetEditor() == nullptr, "Expected host close");
	HE::AssetInspectionSnapshot sceneSnapshot;
	sceneSnapshot.Asset = { .Guid = "scene-guid", .Kind = HE::AssetKind::Scene, .Source = HE::AssetSource::File, .AssetId = "Scene.scene", .AbsolutePath = sourcePath, .ExistsOnDisk = true };
	sceneSnapshot.ImporterId = "scene.native";
	Require(host.Open("scene-guid", [sceneSnapshot](const HE::AssetGuid&, HE::AssetInspectionSnapshot& output) { output = sceneSnapshot; return HE::ResultEnvelope::Success("asset.inspect", "scene-guid", "Inspected"); }).Succeeded(), "Expected scene inspector open");
	Require(dynamic_cast<HE::Editor::SceneAssetEditor*>(host.GetEditor()) != nullptr, "Expected specialized scene inspector");
	host.Close();
	std::filesystem::remove_all(sessionRoot, errorCode);

	std::cout << "AssetEditingSmoke passed" << std::endl;
	return 0;
}
