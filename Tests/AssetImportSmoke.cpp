#include <cstdlib>
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "HuaEngine/Asset/Artifact/MeshArtifact.h"
#include "HuaEngine/Asset/Artifact/MaterialArtifact.h"
#include "HuaEngine/Asset/Artifact/TextureArtifact.h"
#include "HuaEngine/Asset/Artifact/ShaderArtifact.h"
#include "HuaEngine/Asset/Authoring/AssetAuthoringService.h"
#include "HuaEngine/Asset/AssetResolver.h"
#include "HuaEngine/Asset/AssetService.h"
#include "HuaEngine/Asset/Import/AssetImporterRegistry.h"
#include "HuaEngine/Asset/Import/AssetImportService.h"
#include "HuaEngine/Asset/Import/MaterialAssetImporter.h"
#include "HuaEngine/Asset/Import/MeshAssetImporter.h"
#include "HuaEngine/Asset/Import/ObjMeshImporter.h"
#include "HuaEngine/Asset/Import/PngTextureImporter.h"
#include "HuaEngine/Asset/Import/HlslShaderImporter.h"
#include "HuaEngine/Asset/Library/AssetLibrary.h"
#include "HuaEngine/Asset/Metadata/AssetMeta.h"
#include "HuaEngine/Project/ProjectService.h"
#include "HuaEngine/Rendering/Mesh/MeshCore.h"
#include "HuaEngine/Rendering/Material/MaterialSourceData.h"
#include "HuaEngine/Rendering/Material/MaterialSerializer.h"
#include "HuaEngine/Serialization/Serialization.h"

namespace {
	class TransientMeshImporter final : public HE::AssetImporter {
	public:
		explicit TransientMeshImporter(bool& failImport) : m_FailImport(&failImport) {}

		std::string_view GetId() const override { return "smoke.mesh-transient"; }
		uint32_t GetVersion() const override { return 1; }
		uint32_t GetArtifactVersion() const override { return HE::MeshArtifactVersion; }
		bool CanImport(HE::AssetKind kind, std::string_view extension) const override {
			return kind == HE::AssetKind::Mesh && extension == ".transient";
		}
		HE::AssetImportResult Import(const HE::AssetImportContext& context) const override {
			HE::AssetImportResult result;
			if (*m_FailImport) {
				result.Diagnostics.push_back({ HE::DiagnosticSeverity::Error, "smoke.transient_failure", "Transient importer failure", context.SourcePath.generic_string() });
				return result;
			}
			auto mesh = HE::Rendering::Mesh::CreateQuad("TransientMesh");
			if (!mesh || !HE::EncodeMeshArtifact(*mesh, result.Artifact).Succeeded()) return result;
			result.Success = true;
			return result;
		}

	private:
		bool* m_FailImport = nullptr;
	};

	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[AssetImportSmoke] " << message << std::endl;
			std::exit(1);
		}
	}

	void WriteTextFile(const std::filesystem::path& path, const std::string& text);

	class DagTestImporter final : public HE::AssetImporter {
	public:
		explicit DagTestImporter(std::vector<HE::AssetGuid>& importOrder)
			: m_ImportOrder(&importOrder) {}

		std::string_view GetId() const override { return "test.dag"; }
		uint32_t GetVersion() const override { return 1; }
		uint32_t GetArtifactVersion() const override { return HE::TextureArtifactVersion; }
		bool CanImport(HE::AssetKind kind, std::string_view extension) const override {
			return kind == HE::AssetKind::Texture2D && extension == ".dag";
		}
		HE::ResultEnvelope CollectDependencies(const HE::AssetImportContext& context, std::vector<HE::AssetGuid>& output) const override {
			output.clear();
			std::ifstream stream(context.SourcePath);
			if (!stream) return HE::ResultEnvelope::Failure("test.dag.dependencies", context.SourceAsset.Guid, "DAG source could not be read");
			std::string dependency;
			while (std::getline(stream, dependency)) if (!dependency.empty()) output.push_back(dependency);
			return HE::ResultEnvelope::Success("test.dag.dependencies", context.SourceAsset.Guid, "DAG dependencies collected");
		}
		HE::AssetImportResult Import(const HE::AssetImportContext& context) const override {
			HE::AssetImportResult result;
			std::vector<HE::AssetGuid> dependencies;
			if (!CollectDependencies(context, dependencies).Succeeded()) return result;
			HE::TextureArtifactData texture{ .Width = 1, .Height = 1, .Pixels = { 255, 255, 255, 255 } };
			if (!HE::EncodeTextureArtifact(texture, result.Artifact).Succeeded()) return result;
			result.Artifact.Dependencies = std::move(dependencies);
			m_ImportOrder->push_back(context.SourceAsset.Guid);
			result.Success = true;
			return result;
		}

	private:
		std::vector<HE::AssetGuid>* m_ImportOrder = nullptr;
	};

	HE::ProjectContext MakeDagProject(const std::filesystem::path& root) {
		HE::ProjectContext context;
		context.RootPath = root;
		context.ProjectFilePath = root / ".huaengine" / "project.json";
		return context;
	}

	void AddDagRecord(const HE::ProjectContext& context, HE::AssetManifest& manifest, std::string guid) {
		const auto relativePath = std::filesystem::path("Dag") / (guid + ".dag");
		Require(manifest.Upsert({
			.Guid = guid,
			.AssetId = relativePath.generic_string(),
			.Kind = HE::AssetKind::Texture2D,
			.Source = HE::AssetSource::File,
			.RelativePath = relativePath,
			.ImportState = HE::AssetImportState::Registered
		}), "Expected DAG manifest record");
		Require(HE::SaveAssetMeta(context.GetAssetRootPath() / relativePath, { .Guid = guid, .ImporterId = "test.dag" }).Succeeded(), "Expected DAG metadata sidecar");
	}

	void TestGenericImportDag(const std::filesystem::path& root) {
		const auto orderedContext = MakeDagProject(root / "Ordered");
		WriteTextFile(orderedContext.GetAssetRootPath() / "Dag" / "A.dag", "B\n");
		WriteTextFile(orderedContext.GetAssetRootPath() / "Dag" / "B.dag", "C\n");
		WriteTextFile(orderedContext.GetAssetRootPath() / "Dag" / "C.dag", "");
		HE::AssetManifest orderedManifest;
		AddDagRecord(orderedContext, orderedManifest, "A");
		AddDagRecord(orderedContext, orderedManifest, "B");
		AddDagRecord(orderedContext, orderedManifest, "C");
		HE::AssetLibrary orderedLibrary;
		Require(orderedLibrary.Open(orderedContext).Succeeded(), "Expected ordered DAG library open");
		std::vector<HE::AssetGuid> orderedImports;
		HE::AssetImporterRegistry orderedRegistry;
		Require(orderedRegistry.Register(std::make_unique<DagTestImporter>(orderedImports)), "Expected ordered DAG importer registration");
		HE::AssetImportReport orderedReport;
		const std::vector<HE::AssetGuid> orderedRoots{ "A" };
		const auto orderedResult = HE::AssetImportService(orderedRegistry, orderedLibrary).ImportAssets(
			orderedContext,
			orderedManifest,
			orderedRoots,
			HE::AssetImportPolicy::Force,
			&orderedReport);
		Require(orderedResult.Succeeded() && orderedReport.ImportedAssets == 3, "Expected complete DAG import");
		Require(orderedImports == std::vector<HE::AssetGuid>({ "C", "B", "A" }), "Expected stable dependency-first import order");

		const auto cycleContext = MakeDagProject(root / "Cycle");
		WriteTextFile(cycleContext.GetAssetRootPath() / "Dag" / "A.dag", "B\n");
		WriteTextFile(cycleContext.GetAssetRootPath() / "Dag" / "B.dag", "A\n");
		HE::AssetManifest cycleManifest;
		AddDagRecord(cycleContext, cycleManifest, "A");
		AddDagRecord(cycleContext, cycleManifest, "B");
		HE::AssetLibrary cycleLibrary;
		Require(cycleLibrary.Open(cycleContext).Succeeded(), "Expected cycle DAG library open");
		std::vector<HE::AssetGuid> cycleImports;
		HE::AssetImporterRegistry cycleRegistry;
		Require(cycleRegistry.Register(std::make_unique<DagTestImporter>(cycleImports)), "Expected cycle DAG importer registration");
		HE::AssetImportReport cycleReport;
		const std::vector<HE::AssetGuid> cycleRoots{ "A" };
		const auto cycleResult = HE::AssetImportService(cycleRegistry, cycleLibrary).ImportAssets(
			cycleContext,
			cycleManifest,
			cycleRoots,
			HE::AssetImportPolicy::Force,
			&cycleReport);
		Require(cycleResult.Failed() && cycleImports.empty(), "Expected dependency cycle rejection before importer execution");
		Require(cycleLibrary.Find("A") == nullptr && cycleLibrary.Find("B") == nullptr, "Expected dependency cycle rejection before artifact commit");
		const auto cycleDiagnostic = std::find_if(cycleResult.Details.begin(), cycleResult.Details.end(), [](const auto& detail) { return detail.Code == "asset.import.dependency_cycle"; });
		Require(cycleDiagnostic != cycleResult.Details.end(), "Expected stable dependency cycle diagnostic");
		Require(cycleDiagnostic->Message.find("A [") != std::string::npos && cycleDiagnostic->Message.find("B [") != std::string::npos
			&& cycleDiagnostic->Message.find("A.dag") != std::string::npos && cycleDiagnostic->Message.find("B.dag") != std::string::npos,
			"Expected dependency cycle diagnostic to contain GUID and source path chain");
	}

	void TestImporterSelection() {
		HE::AssetImporterRegistry registry;
		Require(registry.Register(std::make_unique<HE::MeshAssetImporter>()), "Expected mesh importer registration");
		Require(registry.Register(std::make_unique<HE::MaterialAssetImporter>()), "Expected material importer registration");
		Require(registry.Register(std::make_unique<HE::PngTextureImporter>()), "Expected PNG importer registration");
		Require(registry.Register(std::make_unique<HE::HlslShaderImporter>()), "Expected HLSL importer registration");

		const auto* importer = registry.Find(HE::AssetKind::Mesh, ".mesh");
		Require(importer != nullptr, "Expected mesh importer lookup");
		Require(importer->GetId() == "hua.mesh-yaml", "Expected stable mesh importer id");
		Require(registry.Find(HE::AssetKind::Mesh, ".MESH") == importer, "Expected case-insensitive extension lookup");
		Require(registry.Find(HE::AssetKind::Material, ".mesh") == nullptr, "Expected kind mismatch to reject importer");
		Require(registry.Find(HE::AssetKind::Mesh, ".obj") == nullptr, "Expected unsupported extension rejection");
		Require(registry.Find(HE::AssetKind::Texture2D, ".PNG") != nullptr, "Expected case-insensitive PNG importer lookup");

		const auto meshMatch = registry.FindByExtension(".MESH");
		Require(meshMatch && meshMatch->Kind == HE::AssetKind::Mesh, "Expected mesh kind inference");
		const auto pngMatch = registry.FindByExtension(".png");
		Require(pngMatch && pngMatch->Kind == HE::AssetKind::Texture2D, "Expected texture kind inference");
		const auto materialMatch = registry.FindByExtension(".MAT");
		Require(materialMatch && materialMatch->Kind == HE::AssetKind::Material, "Expected material kind inference");
		const auto shaderMatch = registry.FindByExtension(".SHADER");
		Require(shaderMatch && shaderMatch->Kind == HE::AssetKind::Shader, "Expected shader kind inference");
		Require(!registry.FindByExtension(".glsl"), "Expected legacy GLSL source rejection");
		Require(!registry.FindByExtension(".obj"), "Expected unsupported extension inference rejection");
	}

	void TestRuntimeCacheInvalidation() {
		HE::AssetRuntimeCache cache;
		cache.StoreMesh("target-guid", HE::Rendering::Mesh::CreateQuad("TargetMesh"));
		cache.StoreMaterial("target-guid", HE::Rendering::Material::Create("TargetMaterial"));
		cache.StoreMesh("other-guid", HE::Rendering::Mesh::CreateQuad("OtherMesh"));

		cache.Invalidate("target-guid");

		Require(!cache.FindMesh("target-guid"), "Expected target mesh cache invalidation");
		Require(!cache.FindMaterial("target-guid"), "Expected target material cache invalidation");
		Require(cache.FindMesh("other-guid") != nullptr, "Expected unrelated cache entry preservation");
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
		Require(firstInitialize.Succeeded(), "Expected first project asset initialization: " + firstInitialize.Summary + (firstInitialize.Details.empty() ? std::string() : " | " + firstInitialize.Details.front().Message));
		Require(firstReport.TotalFileAssets == 1, "Expected one file asset in import report");
		Require(firstReport.TotalBuiltinAssets == 7, "Expected seven builtin assets in import report");
		Require(firstReport.ImportedAssets == 8, "Expected first initialization to import project and builtin assets");
		Require(firstReport.SkippedAssets == 0, "Expected first initialization not to skip mesh");
		const std::array builtinMeshes = {
			std::pair{ HE::BuiltinAssetGuids::QuadMesh, std::filesystem::path("Meshes/Quad.obj") },
			std::pair{ HE::BuiltinAssetGuids::CubeMesh, std::filesystem::path("Meshes/Cube.obj") },
			std::pair{ HE::BuiltinAssetGuids::SphereMesh, std::filesystem::path("Meshes/Sphere.obj") },
			std::pair{ HE::BuiltinAssetGuids::FallbackMesh, std::filesystem::path("Meshes/Fallback.obj") }
		};
		for (const auto& [guid, sourcePath] : builtinMeshes) {
			const auto* libraryRecord = assetService.GetLibrary().Find(guid);
			Require(libraryRecord != nullptr, "Expected builtin mesh artifact: " + guid);
			Require(libraryRecord->ImporterId == "hua.mesh-obj", "Expected builtin mesh to use the OBJ importer: " + guid);
			const auto* manifestRecord = assetService.GetManifest().FindByGuid(guid);
			Require(manifestRecord != nullptr, "Expected builtin mesh manifest record: " + guid);
			Require(manifestRecord->RelativePath == sourcePath, "Expected builtin OBJ source path: " + guid);
		}
		Require(assetService.GetLibrary().Find(HE::BuiltinAssetGuids::DefaultMaterial) != nullptr, "Expected builtin default material artifact");
		Require(assetService.GetLibrary().Find(HE::BuiltinAssetGuids::FallbackMaterial) != nullptr, "Expected builtin fallback material artifact");

		HE::AssetRecord meshRecord;
		Require(assetService.ResolveAsset("Meshes/ImportedQuad.mesh", meshRecord).Succeeded(), "Expected imported mesh record");
		const auto* libraryRecord = assetService.GetLibrary().Find(meshRecord.Guid);
		Require(libraryRecord != nullptr, "Expected mesh library record");
		const auto artifactPath = assetService.GetLibrary().GetRootPath() / libraryRecord->ArtifactRelativePath;
		Require(std::filesystem::is_regular_file(artifactPath), "Expected mesh artifact file");

		const auto firstWriteTime = std::filesystem::last_write_time(artifactPath);
		HE::AssetArtifact firstArtifact;
		Require(assetService.GetLibrary().ReadArtifact(meshRecord.Guid, firstArtifact).Succeeded(), "Expected first mesh artifact read");
		HE::AssetImportReport secondReport;
		Require(assetService.InitializeProjectAssets(context, &secondReport).Succeeded(), "Expected repeated project asset initialization");
		Require(secondReport.ImportedAssets == 0, "Expected repeated initialization not to import mesh");
		Require(secondReport.TotalBuiltinAssets == 7, "Expected repeated initialization to include builtin assets");
		Require(secondReport.SkippedAssets == 8, "Expected repeated initialization to skip compatible project and builtin assets");
		Require(std::filesystem::last_write_time(artifactPath) == firstWriteTime, "Expected skipped mesh artifact not to be rewritten");

		const auto replacementMesh = HE::Rendering::Mesh::CreateQuad("ReimportedQuad");
		Require(HE::Rendering::Mesh::SaveToFile(*replacementMesh, meshPath.generic_string()), "Expected replacement mesh source save");
		HE::AssetImportReport changedSourceReport;
		Require(assetService.InitializeProjectAssets(context, &changedSourceReport).Succeeded(), "Expected changed source project asset initialization");
		Require(changedSourceReport.ImportedAssets == 1, "Expected changed source content to reimport automatically");
		Require(changedSourceReport.SkippedAssets == 7, "Expected unchanged builtin assets to remain skipped");
		HE::AssetArtifact changedSourceArtifact;
		Require(assetService.GetLibrary().ReadArtifact(meshRecord.Guid, changedSourceArtifact).Succeeded(), "Expected changed source artifact read");
		Require(changedSourceArtifact.Payload != firstArtifact.Payload, "Expected changed source content to update the artifact payload");

		HE::AssetImportService importService(assetService.GetImporterRegistry(), assetService.GetLibrary());
		const std::array<HE::AssetGuid, 1> forceGuids = { meshRecord.Guid };
		HE::AssetImportReport forceReport;
		Require(
			importService.ImportAssets(
				context,
				assetService.GetManifest(),
				forceGuids,
				HE::AssetImportPolicy::Force,
				&forceReport).Succeeded(),
			"Expected forced mesh import");
		Require(forceReport.ImportedAssets == 1, "Expected forced import to rewrite an existing artifact");
		Require(forceReport.SkippedAssets == 0, "Expected forced import not to skip a compatible artifact");
		HE::AssetArtifact replacementArtifact;
		Require(assetService.GetLibrary().ReadArtifact(meshRecord.Guid, replacementArtifact).Succeeded(), "Expected replacement artifact read");
		Require(replacementArtifact.Payload != firstArtifact.Payload, "Expected forced import to update artifact payload");

		{
			std::ofstream invalidSource(meshPath, std::ios::out | std::ios::binary | std::ios::trunc);
			Require(invalidSource.good(), "Expected invalid mesh fixture open");
			invalidSource << "not a mesh";
		}
		HE::AssetImportReport failedForceReport;
		Require(
			importService.ImportAssets(
				context,
				assetService.GetManifest(),
				forceGuids,
				HE::AssetImportPolicy::Force,
				&failedForceReport).Succeeded(),
			"Expected batch import to report per-asset failure without infrastructure failure");
		Require(failedForceReport.FailedAssets == 1, "Expected invalid source import failure");
		HE::AssetArtifact preservedArtifact;
		Require(assetService.GetLibrary().ReadArtifact(meshRecord.Guid, preservedArtifact).Succeeded(), "Expected preserved artifact read");
		Require(preservedArtifact.Payload == replacementArtifact.Payload, "Expected failed reimport to preserve the last good artifact");
		Require(HE::Rendering::Mesh::SaveToFile(*replacementMesh, meshPath.generic_string()), "Expected valid source restore after failed reimport");

		const auto* currentLibraryRecord = assetService.GetLibrary().Find(meshRecord.Guid);
		Require(currentLibraryRecord != nullptr, "Expected current mesh library record");
		const auto currentArtifactPath = assetService.GetLibrary().GetRootPath() / currentLibraryRecord->ArtifactRelativePath;
		std::filesystem::remove(currentArtifactPath);
		HE::AssetImportReport rebuildReport;
		Require(assetService.InitializeProjectAssets(context, &rebuildReport).Succeeded(), "Expected missing artifact rebuild");
		Require(rebuildReport.ImportedAssets == 1, "Expected missing artifact to be imported again");
		Require(std::filesystem::is_regular_file(currentArtifactPath), "Expected rebuilt mesh artifact");

		std::filesystem::remove(meshPath);
		assetService.GetRuntimeCache() = HE::AssetRuntimeCache();
		HE::AssetResolver resolver(assetService);
		HE::Ref<HE::Rendering::Mesh> resolvedMesh;
		Require(resolver.ResolveMesh(meshRecord.Guid, resolvedMesh).Succeeded(), "Expected mesh resolve from Library after source removal");
		Require(static_cast<bool>(resolvedMesh), "Expected mesh resolved from artifact");
		Require(resolvedMesh->GetName() == "ReimportedQuad", "Expected Library-only mesh payload");
	}

	void WriteTextFile(const std::filesystem::path& path, const std::string& text) {
		std::filesystem::create_directories(path.parent_path());
		std::ofstream stream(path, std::ios::out | std::ios::binary | std::ios::trunc);
		Require(stream.good(), "Expected text fixture open");
		stream << text;
		Require(stream.good(), "Expected text fixture write");
	}

	void TestObjImportPipeline(const std::filesystem::path& root) {
		HE::ProjectService projectService;
		HE::ProjectContext context;
		Require(projectService.InitializeProject(root, &context, "ObjImportProject").Succeeded(), "Expected OBJ import project initialization");

		const auto quadPath = context.GetAssetRootPath() / "Models" / "ObjQuad.obj";
		WriteTextFile(quadPath,
			"o ObjQuad\n"
			"v -1.0 -1.0 0.0\n"
			"v  1.0 -1.0 0.0\n"
			"v  1.0  1.0 0.0\n"
			"v -1.0  1.0 0.0\n"
			"vt 0.0 0.0\n"
			"vt 1.0 0.0\n"
			"vt 1.0 1.0\n"
			"vt 0.0 1.0\n"
			"vn 0.0 0.0 1.0\n"
			"f -4/-4/1 -3/-3/1 -2/-2/1 -1/-1/1\n");

		HE::AssetService assetService;
		Require(assetService.CanImportSource(quadPath), "Expected OBJ source support query");
		const auto* objImporter = assetService.GetImporterRegistry().Find(HE::AssetKind::Mesh, ".OBJ");
		Require(objImporter != nullptr, "Expected case-insensitive OBJ importer lookup");
		Require(objImporter->GetId() == "hua.mesh-obj", "Expected stable OBJ importer id");

		HE::AssetHandle quadHandle = 0;
		Require(assetService.LoadMeshAsset(context, "Models/ObjQuad.obj", &quadHandle).Succeeded(), "Expected explicit OBJ source registration");
		Require(quadHandle != 0, "Expected explicit OBJ registration handle");
		HE::AssetImportReport initializeReport;
		Require(assetService.InitializeProjectAssets(context, &initializeReport).Succeeded(), "Expected OBJ project asset initialization");
		Require(initializeReport.ImportedAssets == 8, "Expected OBJ and builtin artifacts to import");

		HE::AssetRecord quadRecord;
		Require(assetService.ResolveAsset("Models/ObjQuad.obj", quadRecord).Succeeded(), "Expected OBJ manifest record");
		HE::AssetArtifact quadArtifact;
		Require(assetService.GetLibrary().ReadArtifact(quadRecord.Guid, quadArtifact).Succeeded(), "Expected OBJ mesh artifact read");
		HE::Ref<HE::Rendering::Mesh> quadMesh;
		Require(HE::DecodeMeshArtifact(quadArtifact, quadMesh).Succeeded() && quadMesh, "Expected OBJ mesh artifact decode");
		Require(quadMesh->GetName() == "ObjQuad", "Expected OBJ mesh name from source filename");
		const auto& quadData = quadMesh->GetMeshData();
		Require(quadData.Layout.Stride == 32, "Expected OBJ Position, UV, and normal vertex stride");
		Require(quadData.Layout.Elements.size() == 3, "Expected OBJ Position, UV, and normal layout");
		Require(quadData.Layout.Elements[0].Name == "a_Position", "Expected OBJ position attribute");
		Require(quadData.Layout.Elements[1].Name == "a_TexCoord", "Expected OBJ texture coordinate attribute");
		Require(quadData.Layout.Elements[2].Name == "a_Normal", "Expected OBJ normal attribute");
		Require(quadData.VertexData.size() == 32, "Expected four deduplicated OBJ vertices");
		Require(quadData.IndexData.size() == 6, "Expected OBJ quad triangulation");
		HE::AssetMeta quadMeta;
		Require(HE::LoadAssetMeta(quadPath, quadMeta).Succeeded(), "Expected OBJ metadata");
		HE::ObjMeshImportSettings changedObjSettings;
		changedObjSettings.ImportScale = 2.0f;
		changedObjSettings.UpAxis = HE::MeshAxis::PositiveZ;
		changedObjSettings.ForwardAxis = HE::MeshAxis::NegativeY;
		changedObjSettings.FlipUvV = true;
		changedObjSettings.RecalculateNormals = true;
		changedObjSettings.ReverseWinding = true;
		HE::ObjMeshImporter settingsImporter;
		Require(settingsImporter.EncodeSettings(changedObjSettings, quadMeta.Settings).Succeeded() && HE::SaveAssetMeta(quadPath, quadMeta).Succeeded(), "Expected changed OBJ settings save");
		HE::AssetReimportReport settingsReport;
		Require(assetService.ReimportAssets(context, quadPath, &settingsReport).Succeeded() && settingsReport.ReimportedAssets == 1, "Expected OBJ settings reimport");
		HE::AssetArtifact changedObjArtifact;
		Require(assetService.GetLibrary().ReadArtifact(quadRecord.Guid, changedObjArtifact).Succeeded() && changedObjArtifact.Payload != quadArtifact.Payload, "Expected OBJ settings to change the artifact");
		HE::Ref<HE::Rendering::Mesh> changedObjMesh;
		Require(HE::DecodeMeshArtifact(changedObjArtifact, changedObjMesh).Succeeded(), "Expected changed OBJ artifact decode");
		Require(std::abs(changedObjMesh->GetMeshData().VertexData[0]) == 2.0f, "Expected OBJ import scale to affect positions");

		const auto trianglePath = context.GetAssetRootPath() / "Models" / "NoUvTriangle.obj";
		WriteTextFile(trianglePath,
			"v 0.0 0.0 0.0\n"
			"v 1.0 0.0 0.0\n"
			"v 0.0 1.0 0.0\n"
			"f 1 2 3\n");
		HE::AssetReimportReport noUvReport;
		Require(assetService.ReimportAssets(context, trianglePath, &noUvReport).Succeeded(), "Expected OBJ reimport registration");
		Require(noUvReport.RegisteredAssets == 1 && noUvReport.ReimportedAssets == 1, "Expected missing-UV OBJ import");
		HE::AssetRecord triangleRecord;
		Require(assetService.ResolveAsset("Models/NoUvTriangle.obj", triangleRecord).Succeeded(), "Expected missing-UV OBJ record");
		HE::AssetArtifact triangleArtifact;
		Require(assetService.GetLibrary().ReadArtifact(triangleRecord.Guid, triangleArtifact).Succeeded(), "Expected missing-UV OBJ artifact");
		HE::Ref<HE::Rendering::Mesh> triangleMesh;
		Require(HE::DecodeMeshArtifact(triangleArtifact, triangleMesh).Succeeded() && triangleMesh, "Expected missing-UV OBJ decode");
		const auto& triangleVertices = triangleMesh->GetMeshData().VertexData;
		Require(triangleVertices.size() == 24, "Expected three missing-UV OBJ vertices with generated normals");
		for (size_t vertex = 0; vertex < 3; ++vertex) {
			Require(triangleVertices[vertex * 8 + 3] == 0.0f && triangleVertices[vertex * 8 + 4] == 0.0f, "Expected missing OBJ UVs to default to zero");
		}

		const auto preservedPayload = changedObjArtifact.Payload;
		WriteTextFile(quadPath, "o Invalid\nf 1 2 3\n");
		HE::AssetReimportReport invalidReport;
		Require(assetService.ReimportAssets(context, quadPath, &invalidReport).Succeeded(), "Expected invalid OBJ to report a per-asset failure");
		Require(invalidReport.FailedAssets == 1, "Expected invalid OBJ import failure");
		HE::AssetArtifact preservedArtifact;
		Require(assetService.GetLibrary().ReadArtifact(quadRecord.Guid, preservedArtifact).Succeeded(), "Expected previous OBJ artifact preservation");
		Require(preservedArtifact.Payload == preservedPayload, "Expected invalid OBJ reimport not to replace the previous artifact");
	}

	void WriteBinaryFile(const std::filesystem::path& path, const std::vector<uint8_t>& data) {
		std::filesystem::create_directories(path.parent_path());
		std::ofstream stream(path, std::ios::out | std::ios::binary | std::ios::trunc);
		Require(stream.good(), "Expected binary fixture open");
		stream.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
		Require(stream.good(), "Expected binary fixture write");
	}

	std::vector<uint8_t> MakePngFixtureBytes() {
		return {
			0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
			0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x08, 0x06, 0x00, 0x00, 0x00, 0x72, 0xb6, 0x0d, 0x24,
			0x00, 0x00, 0x00, 0x01, 0x73, 0x52, 0x47, 0x42, 0x00, 0xae, 0xce, 0x1c, 0xe9, 0x00, 0x00, 0x00, 0x04,
			0x67, 0x41, 0x4d, 0x41, 0x00, 0x00, 0xb1, 0x8f, 0x0b, 0xfc, 0x61, 0x05, 0x00, 0x00, 0x00, 0x09,
			0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x0e, 0xc3, 0x00, 0x00, 0x0e, 0xc3, 0x01, 0xc7, 0x6f, 0xa8, 0x64,
			0x00, 0x00, 0x00, 0x16, 0x49, 0x44, 0x41, 0x54, 0x18, 0x57, 0x63, 0xf8, 0xcf, 0xc0, 0xf0, 0x1f, 0x0c,
			0x19, 0x18, 0xfe, 0xff, 0xff, 0x0f, 0x64, 0x00, 0x00, 0x47, 0xca, 0x08, 0xf8, 0x26, 0x7b, 0x18, 0x99,
			0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
		};
	}

	void TestPngTextureImport(const std::filesystem::path& root) {
		const auto pngBytes = MakePngFixtureBytes();
		const auto pngPath = root / "Texture2x2.png";
		WriteBinaryFile(pngPath, pngBytes);

		const HE::ProjectContext projectContext{ .RootPath = root };
		const HE::AssetManifestRecord textureRecord{
			.Guid = "texture-guid-for-png-import",
			.AssetId = "Texture2x2.png",
			.Kind = HE::AssetKind::Texture2D,
			.Source = HE::AssetSource::File,
			.RelativePath = "Texture2x2.png",
			.ImportState = HE::AssetImportState::Registered
		};
		const HE::PngTextureImporter importer;
		const auto defaultSettings = importer.CreateDefaultSettings();
		Require(importer.CanImport(HE::AssetKind::Texture2D, ".png"), "Expected PNG importer selection");
		Require(!importer.CanImport(HE::AssetKind::Texture2D, ".jpg"), "Expected JPG importer rejection");
		Require(!importer.CanImport(HE::AssetKind::Mesh, ".png"), "Expected non-texture PNG rejection");

		const auto importResult = importer.Import({ projectContext, textureRecord, pngPath, nullptr, nullptr, defaultSettings.get() });
		Require(importResult.Success, "Expected 2x2 PNG import");
		HE::TextureArtifactData textureData;
		Require(HE::DecodeTextureArtifact(importResult.Artifact, textureData).Succeeded(), "Expected texture artifact decode");
		Require(textureData.Width == 2 && textureData.Height == 2, "Expected PNG dimensions");
		Require(textureData.Format == HE::TextureArtifactFormat::RGBA8, "Expected forced RGBA8 texture format");
		Require(textureData.MipLevels == 1, "Expected one imported mip");
		const std::vector<uint8_t> expectedFlippedPixels = {
			0, 0, 255, 255, 255, 255, 0, 255,
			255, 0, 0, 255, 0, 255, 0, 255
		};
		Require(textureData.Pixels == expectedFlippedPixels, "Expected importer-owned vertical flip");
		HE::PngTextureImportSettings resizedSettings;
		resizedSettings.MaxSize = 1;
		resizedSettings.AlphaMode = HE::TextureAlphaMode::Opaque;
		const auto resizedResult = importer.Import({ projectContext, textureRecord, pngPath, nullptr, nullptr, &resizedSettings });
		HE::TextureArtifactData resizedTexture;
		Require(resizedResult.Success && HE::DecodeTextureArtifact(resizedResult.Artifact, resizedTexture).Succeeded(), "Expected resized PNG import");
		Require(resizedTexture.Width == 1 && resizedTexture.Height == 1 && resizedTexture.Pixels[3] == 255, "Expected max size and opaque alpha settings to affect the artifact");
		resizedSettings.GenerateMipmaps = true;
		Require(importer.ValidateSettings(resizedSettings).Failed(), "Expected unsupported mipmap setting rejection");

		const auto invalidPath = root / "Invalid.png";
		WriteBinaryFile(invalidPath, { 0x89, 0x50, 0x4e, 0x47 });
		auto invalidRecord = textureRecord;
		invalidRecord.AssetId = "Invalid.png";
		invalidRecord.RelativePath = "Invalid.png";
		Require(!importer.Import({ projectContext, invalidRecord, invalidPath, nullptr, nullptr, defaultSettings.get() }).Success, "Expected invalid PNG rejection");
	}

	void TestMaterialSourceAndArtifact(const std::filesystem::path& root) {
		const auto materialPath = root / "CpuOnly.material";
		WriteTextFile(materialPath,
			"name: CpuOnlyMaterial\n"
			"material_type: Unlit\n"
			"shader_guid: cpu-shader-guid\n"
			"parameters:\n"
			"  u_Int:\n"
			"    value_type: Int\n"
			"    value: -7\n"
			"  u_Float:\n"
			"    value_type: Float\n"
			"    value: 0.5\n"
			"  u_Vec2:\n"
			"    value_type: Vec2\n"
			"    value: { x: 1.0, y: 2.0 }\n"
			"  u_Vec3:\n"
			"    value_type: Vec3\n"
			"    value: { x: 1.0, y: 2.0, z: 3.0 }\n"
			"  u_Color:\n"
			"    value_type: Vec4\n"
			"    value: { x: 0.1, y: 0.2, z: 0.3, w: 1.0 }\n"
			"  u_Mat3:\n"
			"    value_type: Mat3\n"
			"    value: [1, 0, 0, 0, 1, 0, 0, 0, 1]\n"
			"  u_Mat4:\n"
			"    value_type: Mat4\n"
			"    value: [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]\n"
			"  u_Indices:\n"
			"    value_type: IntArray\n"
			"    value: [1, -2, 3]\n"
			"  u_Weights:\n"
			"    value_type: FloatArray\n"
			"    value: [0.25, 0.75]\n"
			"  u_Texture:\n"
			"    value_type: Texture2D\n"
			"    value: Textures/Checker.png\n"
			"  u_EmptyTexture:\n"
			"    value_type: Texture2D\n"
			"    value: ''\n"
			"texture_slots:\n"
			"  u_Texture: 2\n");

		HE::Rendering::MaterialSourceData sourceData;
		Require(HE::Rendering::LoadMaterialSourceData(materialPath, sourceData).Succeeded(), "Expected CPU-only material source parse");
		Require(sourceData.Name == "CpuOnlyMaterial", "Expected material source name");
		Require(sourceData.Type == HE::Rendering::MaterialType::Unlit, "Expected material source type");
		Require(sourceData.Parameters.size() == 11, "Expected all supported material source parameters");
		Require(
			std::get<std::string>(sourceData.Parameters.at("u_Texture").Value) == "Textures/Checker.png",
			"Expected texture source path to remain CPU data");

		HE::Rendering::MaterialSourceData reordered = sourceData;
		reordered.Parameters.clear();
		std::vector<std::string> parameterNames;
		parameterNames.reserve(sourceData.Parameters.size());
		for (const auto& [name, parameter] : sourceData.Parameters) {
			(void)parameter;
			parameterNames.push_back(name);
		}
		std::sort(parameterNames.rbegin(), parameterNames.rend());
		for (const auto& name : parameterNames) reordered.Parameters.emplace(name, sourceData.Parameters.at(name));

		HE::AssetArtifact firstArtifact;
		HE::AssetArtifact secondArtifact;
		Require(HE::EncodeMaterialArtifact(sourceData, firstArtifact).Succeeded(), "Expected material artifact encoding");
		Require(HE::EncodeMaterialArtifact(reordered, secondArtifact).Succeeded(), "Expected reordered material artifact encoding");
		Require(firstArtifact.Payload == secondArtifact.Payload, "Expected deterministic material artifact encoding");

		HE::Rendering::MaterialSourceData decodedData;
		Require(HE::DecodeMaterialArtifact(firstArtifact, decodedData).Succeeded(), "Expected material artifact decoding");
		Require(decodedData.Name == sourceData.Name, "Expected material name round-trip");
		Require(decodedData.Parameters.size() == sourceData.Parameters.size(), "Expected material parameter count round-trip");
		Require(std::get<int>(decodedData.Parameters.at("u_Int").Value) == -7, "Expected signed integer round-trip");
		Require(std::get<std::string>(decodedData.Parameters.at("u_EmptyTexture").Value).empty(), "Expected empty texture round-trip");

		HE::AssetManifest manifest;
		const HE::AssetGuid textureGuid = "texture-guid-for-material-import";
		Require(manifest.Upsert({ .Guid = "cpu-shader-guid", .AssetId = "Shaders/Cpu.shader", .Kind = HE::AssetKind::Shader, .Source = HE::AssetSource::File, .RelativePath = "Shaders/Cpu.shader", .ImportState = HE::AssetImportState::Registered }), "Expected shader manifest fixture");
		Require(manifest.Upsert({
			.Guid = textureGuid,
			.AssetId = "Textures/Checker.png",
			.Kind = HE::AssetKind::Texture2D,
			.Source = HE::AssetSource::File,
			.RelativePath = "Textures/Checker.png",
			.ImportState = HE::AssetImportState::Registered
		}), "Expected texture manifest fixture");
		const HE::AssetManifestRecord materialRecord{
			.Guid = "material-guid-for-import",
			.AssetId = "CpuOnly.material",
			.Kind = HE::AssetKind::Material,
			.Source = HE::AssetSource::File,
			.RelativePath = "CpuOnly.material",
			.ImportState = HE::AssetImportState::Registered
		};
		const HE::ProjectContext projectContext{ .RootPath = root };
		const HE::MaterialAssetImporter importer;
		const auto importResult = importer.Import({ projectContext, materialRecord, materialPath, &manifest });
		Require(importResult.Success, "Expected material importer success");
		Require(importResult.Artifact.Dependencies == std::vector<HE::AssetGuid>({ "cpu-shader-guid", textureGuid }), "Expected material shader and texture dependencies");
		HE::Rendering::MaterialSourceData importedData;
		Require(HE::DecodeMaterialArtifact(importResult.Artifact, importedData).Succeeded(), "Expected imported material decode");
		Require(
			std::get<std::string>(importedData.Parameters.at("u_Texture").Value) == textureGuid,
			"Expected material texture reference converted to guid");

		WriteTextFile(materialPath,
			"name: InvalidTextureMaterial\n"
			"material_type: Unlit\n"
			"shader_guid: ''\n"
			"parameters:\n"
			"  u_Texture:\n"
			"    value_type: Texture2D\n"
			"    value: Textures/Missing.png\n"
			"texture_slots:\n"
			"  u_Texture: 0\n");
		const auto unresolvedTextureResult = importer.Import({ projectContext, materialRecord, materialPath, &manifest });
		Require(!unresolvedTextureResult.Success, "Expected unresolved non-empty material texture reference rejection");
	}

	void TestMaterialImportPipeline(const std::filesystem::path& root) {
		HE::ProjectService projectService;
		HE::ProjectContext context;
		Require(projectService.InitializeProject(root, &context, "MaterialImportProject").Succeeded(), "Expected material import project initialization");

		auto sourceMaterial = HE::Rendering::Material::Create("ImportedMaterial", HE::Rendering::MaterialType::Unlit);
		sourceMaterial->SetShaderProgram(nullptr, HE::BuiltinAssetGuids::UnlitColorShader);
		sourceMaterial->AddParameter({ "u_Color", HE::Rendering::MaterialParameterType::Vec4, glm::vec4(0.2f, 0.4f, 0.6f, 1.0f) });
		const auto materialPath = context.GetAssetRootPath() / "Materials" / "ImportedMaterial.material";
		std::filesystem::create_directories(materialPath.parent_path());
		Require(HE::Serialization::SaveMaterial(*sourceMaterial, materialPath.generic_string()), "Expected material source save");

		HE::AssetService assetService;
		HE::AssetHandle materialHandle = 0;
		Require(assetService.LoadMaterialAsset(context, "Materials/ImportedMaterial.material", &materialHandle).Succeeded(), "Expected material source registration");

		HE::AssetImportReport report;
		Require(assetService.InitializeProjectAssets(context, &report).Succeeded(), "Expected material project asset initialization");
		Require(report.ImportedAssets == 8 && report.FailedAssets == 0, "Expected material and builtin artifact import");
		HE::AssetRecord definitionRecord;
		Require(assetService.ResolveAsset("Materials/ImportedMaterial.material", definitionRecord).Succeeded(), "Expected material definition record");
		HE::Rendering::MaterialDefinition definition;
		Require(assetService.GetMaterialDefinition(definitionRecord.Guid, definition).Succeeded(), "Expected CPU-only material definition query");
		Require(definition.GetShaderGuid() == HE::BuiltinAssetGuids::UnlitColorShader && definition.GetParameters().size() == 1, "Expected shader-authored material definition");
		Require(definition.GetParameters()[0].Name == "u_Color" && std::get<glm::vec4>(definition.GetParameters()[0].CurrentValue) == glm::vec4(0.2f, 0.4f, 0.6f, 1.0f), "Expected material definition current value");
		HE::AssetInspectionSnapshot authoringSnapshot;
		Require(assetService.InspectAsset(definitionRecord.Guid, authoringSnapshot).Succeeded(), "Expected material authoring baseline");
		HE::Rendering::MaterialSourceData editedSource;
		Require(HE::Rendering::LoadMaterialSourceData(materialPath, editedSource).Succeeded(), "Expected material authoring source load");
		editedSource.Parameters.at("u_Color").Value = glm::vec4(0.8f, 0.3f, 0.1f, 1.0f);
		std::string editedText;
		Require(HE::Rendering::EncodeMaterialSourceData(editedSource, editedText).Succeeded(), "Expected material authoring serialization");
		HE::AssetEditCommit editCommit{ .Guid = definitionRecord.Guid, .ExpectedSourceHash = authoringSnapshot.SourceContentHash, .ExpectedMetaHash = authoringSnapshot.MetaContentHash, .SerializedContent = std::vector<uint8_t>(editedText.begin(), editedText.end()) };
		HE::AssetApplyState applyState;
		Require(HE::AssetAuthoringService(assetService).Apply(context, editCommit, applyState).Succeeded() && applyState == HE::AssetApplyState::Applied, "Expected material source authoring apply");
		Require(assetService.GetMaterialDefinition(definitionRecord.Guid, definition).Succeeded() && std::get<glm::vec4>(definition.GetParameters()[0].CurrentValue) == glm::vec4(0.8f, 0.3f, 0.1f, 1.0f), "Expected applied material definition update");
		Require(HE::AssetAuthoringService(assetService).Apply(context, editCommit, applyState).RequiresManualIntervention() && applyState == HE::AssetApplyState::Conflict, "Expected stale authoring baseline conflict");
		WriteTextFile(materialPath, "name: ImportedMaterial\nshader_guid: builtin-shader-unlit-color\nparameters:\n  u_Unknown: 1.0\n");
		HE::AssetReimportReport invalidParameterReport;
		Require(assetService.ReimportAssets(context, materialPath, &invalidParameterReport).Succeeded() && invalidParameterReport.FailedAssets == 1, "Expected unknown shader parameter rejection");
		WriteTextFile(materialPath, "name: ImportedMaterial\nshader_guid: builtin-shader-unlit-color\nparameters:\n  u_Color: 1.0\n");
		HE::AssetReimportReport invalidTypeReport;
		Require(assetService.ReimportAssets(context, materialPath, &invalidTypeReport).Succeeded() && invalidTypeReport.FailedAssets == 1, "Expected material parameter type mismatch rejection");

		HE::AssetRecord record;
		Require(assetService.ResolveAsset("Materials/ImportedMaterial.material", record).Succeeded(), "Expected imported material record");
		const auto* libraryRecord = assetService.GetLibrary().Find(record.Guid);
		Require(libraryRecord && libraryRecord->Kind == HE::AssetKind::Material, "Expected material library record");

		std::filesystem::remove(materialPath);
		assetService.GetRuntimeCache() = HE::AssetRuntimeCache();
		HE::AssetResolver resolver(assetService);
		HE::Ref<HE::Rendering::Material> resolvedMaterial;
		Require(resolver.ResolveMaterial(record.Guid, resolvedMaterial).Succeeded(), "Expected material resolve from Library after source removal");
		Require(resolvedMaterial && resolvedMaterial->GetName() == "ImportedMaterial", "Expected Library-only material payload");
		Require(resolvedMaterial->HasParameter("u_Color"), "Expected material parameter from artifact");
	}

	void TestMaterialShaderGuidDependency(const std::filesystem::path& root) {
		const HE::ProjectContext context{ .RootPath = root };
		const auto materialPath = context.GetAssetRootPath() / "Materials" / "Project.material";
		const auto writeMaterial = [&](std::string_view shaderGuid) {
			WriteTextFile(materialPath,
				"name: ProjectMaterial\n"
				"material_type: Unlit\n"
				"shader_guid: " + std::string(shaderGuid) + "\n"
				"parameters: {}\n"
				"texture_slots: {}\n");
		};
		const HE::AssetManifestRecord materialRecord{
			.Guid = "project-material-guid",
			.AssetId = "Materials/Project.material",
			.Kind = HE::AssetKind::Material,
			.Source = HE::AssetSource::File,
			.RelativePath = "Materials/Project.material",
			.ImportState = HE::AssetImportState::Registered
		};
		const HE::MaterialAssetImporter importer;
		HE::AssetManifest manifest;
		Require(manifest.Upsert({
			.Guid = "project-shader-guid",
			.AssetId = "Shaders/Project.glsl",
			.Kind = HE::AssetKind::Shader,
			.Source = HE::AssetSource::File,
			.RelativePath = "Shaders/Project.glsl",
			.ImportState = HE::AssetImportState::Registered
		}), "Expected shader manifest fixture");
		Require(manifest.Upsert({
			.Guid = "not-a-shader-guid",
			.AssetId = "Meshes/Project.obj",
			.Kind = HE::AssetKind::Mesh,
			.Source = HE::AssetSource::File,
			.RelativePath = "Meshes/Project.obj",
			.ImportState = HE::AssetImportState::Registered
		}), "Expected non-shader manifest fixture");

		writeMaterial("project-shader-guid");
		const auto validResult = importer.Import({ context, materialRecord, materialPath, &manifest });
		Require(validResult.Success, "Expected material shader GUID import");
		Require(validResult.Artifact.Dependencies == std::vector<HE::AssetGuid>{ "project-shader-guid" }, "Expected material shader dependency");
		HE::Rendering::MaterialSourceData importedData;
		Require(HE::DecodeMaterialArtifact(validResult.Artifact, importedData).Succeeded(), "Expected shader GUID material decode");
		Require(importedData.ShaderGuid == "project-shader-guid", "Expected shader GUID to remain in artifact");

		writeMaterial("missing-shader-guid");
		Require(!importer.Import({ context, materialRecord, materialPath, &manifest }).Success, "Expected missing shader GUID rejection");
		writeMaterial("not-a-shader-guid");
		Require(!importer.Import({ context, materialRecord, materialPath, &manifest }).Success, "Expected non-shader GUID rejection");
	}

	void TestAssetReimportPipeline(const std::filesystem::path& root) {
		HE::ProjectService projectService;
		HE::ProjectContext context;
		Require(projectService.InitializeProject(root, &context, "AssetReimportProject").Succeeded(), "Expected reimport project initialization");

		const auto batchDirectory = context.GetAssetRootPath() / "ReimportBatch";
		const auto meshPath = batchDirectory / "Batch.mesh";
		const auto texturePath = batchDirectory / "Nested" / "Batch.png";
		const auto materialPath = batchDirectory / "Batch.mat";
		const auto unsupportedPath = batchDirectory / "Notes.txt";
		std::filesystem::create_directories(batchDirectory);
		Require(HE::Rendering::Mesh::SaveToFile(*HE::Rendering::Mesh::CreateQuad("BatchMesh"), meshPath.generic_string()), "Expected batch mesh fixture");
		WriteBinaryFile(texturePath, MakePngFixtureBytes());
		auto sourceMaterial = HE::Rendering::Material::Create("BatchMaterial", HE::Rendering::MaterialType::Unlit);
		sourceMaterial->SetShaderProgram(nullptr, HE::BuiltinAssetGuids::UnlitColorShader);
		sourceMaterial->AddParameter({ "u_Color", HE::Rendering::MaterialParameterType::Vec4, glm::vec4(0.3f, 0.6f, 0.9f, 1.0f) });
		Require(HE::Serialization::SaveMaterial(*sourceMaterial, materialPath.generic_string()), "Expected batch material fixture");
		WriteTextFile(unsupportedPath, "unsupported");

		HE::AssetService assetService;
		assetService.GetRuntimeCache().StoreMesh("unrelated-guid", HE::Rendering::Mesh::CreateQuad("UnrelatedMesh"));
		Require(assetService.CanImportSource(meshPath), "Expected mesh source support query");
		Require(assetService.CanImportSource(texturePath), "Expected PNG source support query");
		Require(!assetService.CanImportSource(unsupportedPath), "Expected unsupported source query rejection");

		HE::AssetReimportReport firstReport;
		const auto firstResult = assetService.ReimportAssets(context, batchDirectory, &firstReport);
		Require(firstResult.Succeeded(), "Expected directory reimport success");
		Require(firstReport.ScannedFiles == 4, "Expected recursive scan count");
		Require(firstReport.SupportedFiles == 3, "Expected supported asset count");
		Require(firstReport.RegisteredAssets == 0, "Expected metadata discovery to register assets before reimport");
		Require(firstReport.ReimportedAssets == 3, "Expected supported assets to be imported");
		Require(firstReport.SkippedFiles == 1, "Expected unsupported file skip");
		Require(assetService.GetRuntimeCache().FindMesh("unrelated-guid") != nullptr, "Expected initial reimport to preserve unrelated runtime cache entries");

		HE::AssetRecord meshRecord;
		HE::AssetRecord textureRecord;
		HE::AssetRecord materialRecord;
		Require(assetService.ResolveAsset("ReimportBatch/Batch.mesh", meshRecord).Succeeded(), "Expected auto-registered mesh record");
		Require(assetService.ResolveAsset("ReimportBatch/Nested/Batch.png", textureRecord).Succeeded(), "Expected auto-registered texture record");
		Require(assetService.ResolveAsset("ReimportBatch/Batch.mat", materialRecord).Succeeded(), "Expected auto-registered material record");
		const auto stableMeshGuid = meshRecord.Guid;
		const auto stableTextureGuid = textureRecord.Guid;
		const auto stableMaterialGuid = materialRecord.Guid;

		HE::AssetReimportReport secondReport;
		Require(assetService.ReimportAssets(context, batchDirectory, &secondReport).Succeeded(), "Expected repeated directory reimport success");
		Require(secondReport.RegisteredAssets == 0, "Expected repeated reimport not to register assets again");
		Require(assetService.ResolveAsset("ReimportBatch/Batch.mesh", meshRecord).Succeeded() && meshRecord.Guid == stableMeshGuid, "Expected stable mesh GUID after reimport");
		Require(assetService.ResolveAsset("ReimportBatch/Nested/Batch.png", textureRecord).Succeeded() && textureRecord.Guid == stableTextureGuid, "Expected stable texture GUID after reimport");
		Require(assetService.ResolveAsset("ReimportBatch/Batch.mat", materialRecord).Succeeded() && materialRecord.Guid == stableMaterialGuid, "Expected stable material GUID after reimport");

		HE::AssetResolver resolver(assetService);
		HE::Ref<HE::Rendering::Mesh> firstMesh;
		Require(resolver.ResolveMesh(stableMeshGuid, firstMesh).Succeeded() && firstMesh, "Expected imported mesh resolve");
		Require(firstMesh->GetName() == "BatchMesh", "Expected original imported mesh name");
		Require(HE::Rendering::Mesh::SaveToFile(*HE::Rendering::Mesh::CreateQuad("UpdatedBatchMesh"), meshPath.generic_string()), "Expected updated mesh fixture");

		HE::AssetReimportReport meshReport;
		Require(assetService.ReimportAssets(context, meshPath, &meshReport).Succeeded(), "Expected single mesh reimport success");
		Require(meshReport.ReimportedAssets == 1, "Expected single mesh artifact update");
		Require(!assetService.GetRuntimeCache().FindMesh(stableMeshGuid), "Expected successful reimport to invalidate runtime mesh cache");
		HE::Ref<HE::Rendering::Mesh> updatedMesh;
		Require(resolver.ResolveMesh(stableMeshGuid, updatedMesh).Succeeded() && updatedMesh, "Expected updated mesh resolve");
		Require(updatedMesh->GetName() == "UpdatedBatchMesh", "Expected updated mesh artifact payload");

		WriteTextFile(meshPath, "invalid mesh");
		HE::AssetReimportReport failedReport;
		Require(assetService.ReimportAssets(context, meshPath, &failedReport).Succeeded(), "Expected per-asset reimport failure report");
		Require(failedReport.FailedAssets == 1, "Expected invalid mesh import failure");
		Require(assetService.GetRuntimeCache().FindMesh(stableMeshGuid) == updatedMesh, "Expected failed reimport to preserve runtime cache");

		const auto dependencyDirectory = context.GetAssetRootPath() / "ReimportDependencies";
		const auto shaderPath = dependencyDirectory / "Shared.shader";
		const auto shaderSourcePath = dependencyDirectory / "Shared.hlsl";
		const auto dependentMaterialPath = dependencyDirectory / "Dependent.material";
		WriteTextFile(shaderPath, "name: Shared\nlanguage: HLSL\nsource: Shared.hlsl\nstages:\n  vertex: { entry: VSMain, profile: vs_6_0 }\n  fragment: { entry: PSMain, profile: ps_6_0 }\nparameters: {}\n");
		WriteTextFile(shaderSourcePath, "struct V { float4 Position : SV_Position; }; V VSMain(float3 p : POSITION) { V o; o.Position=float4(p,1); return o; } float4 PSMain(V i) : SV_Target0 { return float4(1,1,1,1); }");
		Require(assetService.ReimportAssets(context, shaderPath).Succeeded(), "Expected dependency shader reimport");
		HE::AssetRecord shaderRecord;
		Require(assetService.ResolveAsset("ReimportDependencies/Shared.shader", shaderRecord).Succeeded(), "Expected dependency shader record");
		WriteTextFile(dependentMaterialPath,
			"name: DependentMaterial\n"
			"material_type: Unlit\n"
			"shader_guid: " + shaderRecord.Guid + "\n"
			"parameters: {}\n");
		Require(assetService.ReimportAssets(context, dependentMaterialPath).Succeeded(), "Expected dependent material reimport");
		HE::AssetRecord dependentMaterialRecord;
		Require(assetService.ResolveAsset("ReimportDependencies/Dependent.material", dependentMaterialRecord).Succeeded(), "Expected dependent material record");
		const auto initialDependentFingerprint = assetService.GetLibrary().Find(dependentMaterialRecord.Guid)->ImportFingerprint;
		assetService.GetRuntimeCache().StoreMaterial(
			dependentMaterialRecord.Guid,
			HE::Rendering::Material::Create("CachedDependentMaterial", HE::Rendering::MaterialType::Unlit));
		WriteTextFile(shaderSourcePath, "not valid hlsl");
		HE::AssetReimportReport failedShaderReport;
		Require(
			assetService.ReimportAssets(context, shaderPath, &failedShaderReport).Succeeded() && failedShaderReport.FailedAssets == 1,
			"Expected shader compilation failure to remain a per-asset reimport failure");
		HE::Rendering::MaterialDefinition lastGoodDefinition;
		HE::AssetImportHealth lastGoodHealth;
		Require(
			assetService.GetMaterialDefinition(dependentMaterialRecord.Guid, lastGoodDefinition, &lastGoodHealth).Succeeded(),
			"Expected material definition to remain available from last-good shader artifacts");
		Require(
			lastGoodHealth.State == HE::AssetImportHealthState::LastGoodWithFailure && !lastGoodHealth.Diagnostics.empty(),
			"Expected last-good material health to retain shader import diagnostics");
		HE::AssetService restartedAssetService;
		HE::AssetImportReport restartedImportReport;
		Require(
			restartedAssetService.InitializeProjectAssets(context, &restartedImportReport).Succeeded() && restartedImportReport.FailedAssets >= 1,
			"Expected project startup to retry the invalid shader import");
		HE::Rendering::MaterialDefinition restartedLastGoodDefinition;
		HE::AssetImportHealth restartedLastGoodHealth;
		Require(
			restartedAssetService.GetMaterialDefinition(dependentMaterialRecord.Guid, restartedLastGoodDefinition, &restartedLastGoodHealth).Succeeded() &&
				restartedLastGoodHealth.State == HE::AssetImportHealthState::LastGoodWithFailure,
			"Expected project startup import failure to preserve visible last-good health");

		WriteTextFile(shaderSourcePath, "struct V { float4 Position : SV_Position; }; V VSMain(float3 p : POSITION) { V o; o.Position=float4(p,1); return o; } float4 PSMain(V i) : SV_Target0 { return float4(0.5,1,1,1); }");
		Require(assetService.ReimportAssets(context, shaderPath).Succeeded(), "Expected dependency shader update");
		HE::AssetImportHealth currentShaderHealth;
		Require(
			assetService.GetAssetImportHealth(shaderRecord.Guid, currentShaderHealth).Succeeded() && currentShaderHealth.State == HE::AssetImportHealthState::Current,
			"Expected successful shader reimport to clear previous failure health");

		bool transientImportFailure = false;
		Require(assetService.GetImporterRegistry().Register(std::make_unique<TransientMeshImporter>(transientImportFailure)), "Expected transient smoke importer registration");
		const auto transientPath = context.GetAssetRootPath() / "Transient" / "Stable.transient";
		WriteTextFile(transientPath, "stable source fingerprint");
		HE::AssetReimportReport transientInitialReport;
		Require(assetService.ReimportAssets(context, transientPath, &transientInitialReport).Succeeded() && transientInitialReport.ReimportedAssets == 1, "Expected initial transient mesh import");
		HE::AssetRecord transientRecord;
		Require(assetService.ResolveAsset("Transient/Stable.transient", transientRecord).Succeeded(), "Expected transient mesh record");
		transientImportFailure = true;
		HE::AssetReimportReport transientFailureReport;
		Require(assetService.ReimportAssets(context, transientPath, &transientFailureReport).Succeeded() && transientFailureReport.FailedAssets == 1, "Expected same-fingerprint transient import failure");
		HE::AssetImportHealth transientFailureHealth;
		Require(assetService.GetAssetImportHealth(transientRecord.Guid, transientFailureHealth).Succeeded() && transientFailureHealth.State == HE::AssetImportHealthState::LastGoodWithFailure, "Expected in-memory transient failure health");

		transientImportFailure = false;
		HE::AssetService transientRestartedService;
		Require(transientRestartedService.GetImporterRegistry().Register(std::make_unique<TransientMeshImporter>(transientImportFailure)), "Expected restarted transient smoke importer registration");
		HE::AssetImportReport transientRestartReport;
		Require(transientRestartedService.InitializeProjectAssets(context, &transientRestartReport).Succeeded(), "Expected project restart with current transient artifact");
		HE::AssetImportHealth persistedTransientHealth;
		Require(
			transientRestartedService.GetAssetImportHealth(transientRecord.Guid, persistedTransientHealth).Succeeded() &&
				persistedTransientHealth.State == HE::AssetImportHealthState::LastGoodWithFailure &&
				!persistedTransientHealth.Diagnostics.empty(),
			"Expected same-fingerprint transient import failure to survive restart");
		HE::AssetInspectionSnapshot shaderInspection;
		Require(assetService.InspectAsset(shaderRecord.Guid, shaderInspection).Succeeded() && shaderInspection.ShaderData.has_value(), "Expected shader artifact inspection data");
		HE::AssetImportHealth missingHealth;
		Require(
			assetService.GetAssetImportHealth("missing-asset-guid", missingHealth).Failed() && missingHealth.State == HE::AssetImportHealthState::Missing,
			"Expected unknown asset health to report Missing");
		const std::string shaderDescriptor = "name: Shared\nlanguage: HLSL\nsource: Shared.hlsl\nstages:\n  vertex: { entry: VSMain, profile: vs_6_0 }\n  fragment: { entry: PSMain, profile: ps_6_0 }\nparameters: {}\n";
		Require(std::filesystem::remove(shaderPath), "Expected shader descriptor removal for stale health test");
		HE::AssetImportHealth staleShaderHealth;
		Require(
			assetService.GetAssetImportHealth(shaderRecord.Guid, staleShaderHealth).Succeeded() && staleShaderHealth.State == HE::AssetImportHealthState::Stale,
			"Expected missing source with an existing artifact to report Stale");
		WriteTextFile(shaderPath, shaderDescriptor);
		Require(!assetService.GetRuntimeCache().FindMaterial(dependentMaterialRecord.Guid), "Expected shader reimport to invalidate dependent material cache");
		Require(assetService.GetLibrary().Find(dependentMaterialRecord.Guid)->ImportFingerprint == initialDependentFingerprint, "Expected implementation-only shader change not to reimport material");
		WriteTextFile(shaderPath, "name: Shared\nlanguage: HLSL\nsource: Shared.hlsl\nstages:\n  vertex: { entry: VSMain, profile: vs_6_0 }\n  fragment: { entry: PSMain, profile: ps_6_0 }\nparameters:\n  u_Value:\n    scope: Material\n    editor: Color\n    default: [1.0, 1.0, 1.0, 1.0]\n");
		WriteTextFile(shaderSourcePath, "[[vk::binding(0,1)]] cbuffer MaterialData : register(b0, space1) { float4 u_Value; }; struct V { float4 Position : SV_Position; }; V VSMain(float3 p : POSITION) { V o; o.Position=float4(p,1); return o; } float4 PSMain(V i) : SV_Target0 { return u_Value; }");
		HE::AssetReimportReport interfaceReport;
		const auto interfaceResult = assetService.ReimportAssets(context, shaderPath, &interfaceReport);
		Require(interfaceResult.Succeeded() && interfaceReport.FailedAssets == 0, "Expected shader interface update");
		Require(assetService.GetLibrary().Find(dependentMaterialRecord.Guid)->ImportFingerprint != initialDependentFingerprint, "Expected shader interface change to reimport dependent material");

		const auto outsidePath = context.RootPath / "Outside.mesh";
		Require(HE::Rendering::Mesh::SaveToFile(*HE::Rendering::Mesh::CreateQuad("Outside"), outsidePath.generic_string()), "Expected outside mesh fixture");
		const auto manifestSize = assetService.GetManifest().Size();
		Require(assetService.ReimportAssets(context, outsidePath).Failed(), "Expected path outside Assets rejection");
		Require(assetService.GetManifest().Size() == manifestSize, "Expected outside path not to mutate manifest");
		Require(assetService.ReimportAssets(context, unsupportedPath).RequiresManualIntervention(), "Expected unsupported single file manual intervention");
	}

	void TestBuiltinResolverRequiresLibraryArtifacts(const std::filesystem::path& root) {
		HE::AssetService uninitializedService;
		HE::AssetResolver uninitializedResolver(uninitializedService);
		HE::Ref<HE::Rendering::Mesh> uninitializedMesh;
		Require(
			uninitializedResolver.ResolveMesh(HE::BuiltinAssetGuids::QuadMesh, uninitializedMesh).Failed(),
			"Expected builtin resolution to require initialized project assets");

		HE::ProjectService projectService;
		HE::ProjectContext context;
		Require(projectService.InitializeProject(root, &context, "BuiltinArtifactProject").Succeeded(), "Expected builtin artifact project initialization");

		HE::AssetService assetService;
		Require(assetService.InitializeProjectAssets(context).Succeeded(), "Expected builtin artifacts to initialize");
		const auto* meshRecord = assetService.GetLibrary().Find(HE::BuiltinAssetGuids::QuadMesh);
		const auto* materialRecord = assetService.GetLibrary().Find(HE::BuiltinAssetGuids::DefaultMaterial);
		Require(meshRecord != nullptr, "Expected builtin mesh Library record");
		Require(materialRecord != nullptr, "Expected builtin material Library record");

		const auto meshArtifactPath = assetService.GetLibrary().GetRootPath() / meshRecord->ArtifactRelativePath;
		const auto materialArtifactPath = assetService.GetLibrary().GetRootPath() / materialRecord->ArtifactRelativePath;
		Require(std::filesystem::remove(meshArtifactPath), "Expected builtin mesh artifact removal");
		Require(std::filesystem::remove(materialArtifactPath), "Expected builtin material artifact removal");
		assetService.GetRuntimeCache() = HE::AssetRuntimeCache();

		HE::AssetResolver resolver(assetService);
		HE::Ref<HE::Rendering::Mesh> mesh;
		HE::Ref<HE::Rendering::Material> material;
		Require(resolver.ResolveMesh(HE::BuiltinAssetGuids::QuadMesh, mesh).Failed(), "Expected missing builtin mesh artifact not to use a runtime factory");
		Require(resolver.ResolveMaterial(HE::BuiltinAssetGuids::DefaultMaterial, material).Failed(), "Expected missing builtin material artifact not to use a runtime factory");
	}
}

int main() {
	HE::Log::Init({ .EnableConsoleOutput = false });
	HE::Serialization::InitializeSerialization();

	TestImporterSelection();
	TestRuntimeCacheInvalidation();
	TestMeshArtifactRoundTrip();

	const auto smokeRoot = std::filesystem::temp_directory_path() / "HuaEngineAssetImportSmoke";
	std::error_code errorCode;
	std::filesystem::remove_all(smokeRoot, errorCode);
	Require(!errorCode, "Expected import smoke cleanup before test");
	TestMeshImportPipeline(smokeRoot / "Project");
	TestMaterialSourceAndArtifact(smokeRoot / "MaterialSource");
	TestMaterialImportPipeline(smokeRoot / "MaterialProject");
	TestMaterialShaderGuidDependency(smokeRoot / "MaterialShaderProject");
	TestPngTextureImport(smokeRoot / "TextureSource");
	TestObjImportPipeline(smokeRoot / "ObjProject");
	TestGenericImportDag(smokeRoot / "DagProject");
	TestAssetReimportPipeline(smokeRoot / "ReimportProject");
	TestBuiltinResolverRequiresLibraryArtifacts(smokeRoot / "BuiltinArtifactProject");
	std::filesystem::remove_all(smokeRoot, errorCode);
	Require(!errorCode, "Expected import smoke cleanup after test");

	std::cout << "AssetImportSmoke passed" << std::endl;
	return 0;
}
