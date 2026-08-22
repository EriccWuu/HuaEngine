#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include "HuaEngine/Asset/Artifact/MeshArtifact.h"
#include "HuaEngine/Asset/AssetResolver.h"
#include "HuaEngine/Asset/AssetService.h"
#include "HuaEngine/Asset/Import/AssetImporterRegistry.h"
#include "HuaEngine/Asset/Import/MeshAssetImporter.h"
#include "HuaEngine/Project/ProjectService.h"
#include "HuaEngine/Rendering/Mesh/MeshCore.h"
#include "HuaEngine/Serialization/Serialization.h"

namespace {
	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[AssetImportSmoke] " << message << std::endl;
			std::exit(1);
		}
	}

	void TestImporterSelection() {
		HE::AssetImporterRegistry registry;
		Require(registry.Register(std::make_unique<HE::MeshAssetImporter>()), "Expected mesh importer registration");

		const auto* importer = registry.Find(HE::AssetKind::Mesh, ".mesh");
		Require(importer != nullptr, "Expected mesh importer lookup");
		Require(importer->GetId() == "hua.mesh-yaml", "Expected stable mesh importer id");
		Require(registry.Find(HE::AssetKind::Mesh, ".MESH") == importer, "Expected case-insensitive extension lookup");
		Require(registry.Find(HE::AssetKind::Material, ".mesh") == nullptr, "Expected kind mismatch to reject importer");
		Require(registry.Find(HE::AssetKind::Mesh, ".obj") == nullptr, "Expected unsupported extension rejection");
	}

	void TestMeshArtifactRoundTrip() {
		const auto sourceMesh = HE::Rendering::Mesh::CreateQuad("ArtifactQuad");
		Require(static_cast<bool>(sourceMesh), "Expected source mesh creation");

		HE::AssetArtifact artifact;
		Require(HE::EncodeMeshArtifact(*sourceMesh, artifact).Succeeded(), "Expected mesh artifact encoding");
		Require(artifact.Kind == HE::AssetKind::Mesh, "Expected mesh artifact kind");
		Require(artifact.ArtifactVersion == HE::MeshArtifactVersion, "Expected mesh artifact version");

		HE::Ref<HE::Rendering::Mesh> decodedMesh;
		Require(HE::DecodeMeshArtifact(artifact, decodedMesh).Succeeded(), "Expected mesh artifact decoding");
		Require(static_cast<bool>(decodedMesh), "Expected decoded mesh");
		Require(decodedMesh->GetName() == sourceMesh->GetName(), "Expected mesh name round-trip");

		const auto& sourceData = sourceMesh->GetMeshData();
		const auto& decodedData = decodedMesh->GetMeshData();
		Require(decodedData.VertexData == sourceData.VertexData, "Expected vertex data round-trip");
		Require(decodedData.IndexData == sourceData.IndexData, "Expected index data round-trip");
		Require(decodedData.Layout.Stride == sourceData.Layout.Stride, "Expected layout stride round-trip");
		Require(decodedData.Layout.Elements.size() == sourceData.Layout.Elements.size(), "Expected layout element count round-trip");
		for (size_t index = 0; index < sourceData.Layout.Elements.size(); ++index) {
			const auto& sourceElement = sourceData.Layout.Elements[index];
			const auto& decodedElement = decodedData.Layout.Elements[index];
			Require(decodedElement.Type == sourceElement.Type, "Expected layout type round-trip");
			Require(decodedElement.Name == sourceElement.Name, "Expected layout name round-trip");
			Require(decodedElement.Size == sourceElement.Size, "Expected layout size round-trip");
			Require(decodedElement.Offset == sourceElement.Offset, "Expected layout offset round-trip");
			Require(decodedElement.Normalized == sourceElement.Normalized, "Expected layout normalized round-trip");
		}
	}

	void TestMeshImportPipeline(const std::filesystem::path& root) {
		HE::ProjectService projectService;
		HE::ProjectContext context;
		Require(projectService.InitializeProject(root, &context, "AssetImportProject").Succeeded(), "Expected import test project initialization");

		const auto meshPath = context.GetAssetRootPath() / "Meshes" / "ImportedQuad.mesh";
		std::filesystem::create_directories(meshPath.parent_path());
		const auto sourceMesh = HE::Rendering::Mesh::CreateQuad("ImportedQuad");
		Require(HE::Rendering::Mesh::SaveToFile(*sourceMesh, meshPath.generic_string()), "Expected mesh source save");

		HE::AssetService assetService;
		HE::AssetHandle meshHandle = 0;
		Require(assetService.LoadMeshAsset(context, "Meshes/ImportedQuad.mesh", &meshHandle).Succeeded(), "Expected source mesh registration");
		Require(meshHandle != 0, "Expected registered mesh handle");

		HE::AssetImportReport firstReport;
		const auto firstInitialize = assetService.InitializeProjectAssets(context, &firstReport);
		Require(firstInitialize.Succeeded(), "Expected first project asset initialization");
		Require(firstReport.TotalFileAssets == 1, "Expected one file asset in import report");
		Require(firstReport.ImportedAssets == 1, "Expected first initialization to import mesh");
		Require(firstReport.SkippedAssets == 0, "Expected first initialization not to skip mesh");

		HE::AssetRecord meshRecord;
		Require(assetService.ResolveAsset("Meshes/ImportedQuad.mesh", meshRecord).Succeeded(), "Expected imported mesh record");
		const auto* libraryRecord = assetService.GetLibrary().Find(meshRecord.Guid);
		Require(libraryRecord != nullptr, "Expected mesh library record");
		const auto artifactPath = assetService.GetLibrary().GetRootPath() / libraryRecord->ArtifactRelativePath;
		Require(std::filesystem::is_regular_file(artifactPath), "Expected mesh artifact file");

		const auto firstWriteTime = std::filesystem::last_write_time(artifactPath);
		HE::AssetImportReport secondReport;
		Require(assetService.InitializeProjectAssets(context, &secondReport).Succeeded(), "Expected repeated project asset initialization");
		Require(secondReport.ImportedAssets == 0, "Expected repeated initialization not to import mesh");
		Require(secondReport.SkippedAssets == 1, "Expected repeated initialization to skip compatible mesh");
		Require(std::filesystem::last_write_time(artifactPath) == firstWriteTime, "Expected skipped mesh artifact not to be rewritten");

		std::filesystem::remove(artifactPath);
		HE::AssetImportReport rebuildReport;
		Require(assetService.InitializeProjectAssets(context, &rebuildReport).Succeeded(), "Expected missing artifact rebuild");
		Require(rebuildReport.ImportedAssets == 1, "Expected missing artifact to be imported again");
		Require(std::filesystem::is_regular_file(artifactPath), "Expected rebuilt mesh artifact");

		std::filesystem::remove(meshPath);
		assetService.GetRuntimeCache() = HE::AssetRuntimeCache();
		HE::AssetResolver resolver(assetService);
		HE::Ref<HE::Rendering::Mesh> resolvedMesh;
		Require(resolver.ResolveMesh(meshRecord.Guid, resolvedMesh).Succeeded(), "Expected mesh resolve from Library after source removal");
		Require(static_cast<bool>(resolvedMesh), "Expected mesh resolved from artifact");
		Require(resolvedMesh->GetName() == "ImportedQuad", "Expected Library-only mesh payload");
	}
}

int main() {
	HE::Log::Init({ .EnableConsoleOutput = false });
	HE::Serialization::InitializeSerialization();

	TestImporterSelection();
	TestMeshArtifactRoundTrip();

	const auto smokeRoot = std::filesystem::temp_directory_path() / "HuaEngineAssetImportSmoke";
	std::error_code errorCode;
	std::filesystem::remove_all(smokeRoot, errorCode);
	Require(!errorCode, "Expected import smoke cleanup before test");
	TestMeshImportPipeline(smokeRoot / "Project");
	std::filesystem::remove_all(smokeRoot, errorCode);
	Require(!errorCode, "Expected import smoke cleanup after test");

	std::cout << "AssetImportSmoke passed" << std::endl;
	return 0;
}
