#include <cstdlib>
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include "HuaEngine/Asset/Artifact/MeshArtifact.h"
#include "HuaEngine/Asset/Artifact/MaterialArtifact.h"
#include "HuaEngine/Asset/Artifact/TextureArtifact.h"
#include "HuaEngine/Asset/AssetResolver.h"
#include "HuaEngine/Asset/AssetService.h"
#include "HuaEngine/Asset/Import/AssetImporterRegistry.h"
#include "HuaEngine/Asset/Import/MaterialAssetImporter.h"
#include "HuaEngine/Asset/Import/MeshAssetImporter.h"
#include "HuaEngine/Asset/Import/PngTextureImporter.h"
#include "HuaEngine/Project/ProjectService.h"
#include "HuaEngine/Rendering/Mesh/MeshCore.h"
#include "HuaEngine/Rendering/Material/MaterialSourceData.h"
#include "HuaEngine/Rendering/Material/MaterialSerializer.h"
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
		Require(registry.Register(std::make_unique<HE::MaterialAssetImporter>()), "Expected material importer registration");
		Require(registry.Register(std::make_unique<HE::PngTextureImporter>()), "Expected PNG importer registration");

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
		Require(firstInitialize.Succeeded(), "Expected first project asset initialization");
		Require(firstReport.TotalFileAssets == 1, "Expected one file asset in import report");
		Require(firstReport.TotalBuiltinAssets == 6, "Expected six builtin assets in import report");
		Require(firstReport.ImportedAssets == 7, "Expected first initialization to import project and builtin assets");
		Require(firstReport.SkippedAssets == 0, "Expected first initialization not to skip mesh");
		Require(assetService.GetLibrary().Find(HE::BuiltinAssetGuids::QuadMesh) != nullptr, "Expected builtin quad artifact");
		Require(assetService.GetLibrary().Find(HE::BuiltinAssetGuids::CubeMesh) != nullptr, "Expected builtin cube artifact");
		Require(assetService.GetLibrary().Find(HE::BuiltinAssetGuids::SphereMesh) != nullptr, "Expected builtin sphere artifact");
		Require(assetService.GetLibrary().Find(HE::BuiltinAssetGuids::FallbackMesh) != nullptr, "Expected builtin fallback mesh artifact");
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
		Require(secondReport.TotalBuiltinAssets == 6, "Expected repeated initialization to include builtin assets");
		Require(secondReport.SkippedAssets == 7, "Expected repeated initialization to skip compatible project and builtin assets");
		Require(std::filesystem::last_write_time(artifactPath) == firstWriteTime, "Expected skipped mesh artifact not to be rewritten");

		const auto replacementMesh = HE::Rendering::Mesh::CreateQuad("ReimportedQuad");
		Require(HE::Rendering::Mesh::SaveToFile(*replacementMesh, meshPath.generic_string()), "Expected replacement mesh source save");
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
		Require(resolvedMesh->GetName() == "ReimportedQuad", "Expected Library-only mesh payload");
	}

	void WriteTextFile(const std::filesystem::path& path, const std::string& text) {
		std::filesystem::create_directories(path.parent_path());
		std::ofstream stream(path, std::ios::out | std::ios::binary | std::ios::trunc);
		Require(stream.good(), "Expected text fixture open");
		stream << text;
		Require(stream.good(), "Expected text fixture write");
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
		Require(importer.CanImport(HE::AssetKind::Texture2D, ".png"), "Expected PNG importer selection");
		Require(!importer.CanImport(HE::AssetKind::Texture2D, ".jpg"), "Expected JPG importer rejection");
		Require(!importer.CanImport(HE::AssetKind::Mesh, ".png"), "Expected non-texture PNG rejection");

		const auto importResult = importer.Import({ projectContext, textureRecord, pngPath, nullptr });
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

		const auto invalidPath = root / "Invalid.png";
		WriteBinaryFile(invalidPath, { 0x89, 0x50, 0x4e, 0x47 });
		auto invalidRecord = textureRecord;
		invalidRecord.AssetId = "Invalid.png";
		invalidRecord.RelativePath = "Invalid.png";
		Require(!importer.Import({ projectContext, invalidRecord, invalidPath, nullptr }).Success, "Expected invalid PNG rejection");
	}

	void TestMaterialSourceAndArtifact(const std::filesystem::path& root) {
		const auto materialPath = root / "CpuOnly.material";
		WriteTextFile(materialPath,
			"name: CpuOnlyMaterial\n"
			"material_type: Unlit\n"
			"shader_path: ''\n"
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
		Require(sourceData.TextureSlots.at("u_Texture") == 2, "Expected material texture slot");

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
		Require(decodedData.TextureSlots == sourceData.TextureSlots, "Expected material slots round-trip");

		HE::AssetManifest manifest;
		const HE::AssetGuid textureGuid = "texture-guid-for-material-import";
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
		Require(importResult.Artifact.Dependencies == std::vector<HE::AssetGuid>{ textureGuid }, "Expected material texture dependency");
		HE::Rendering::MaterialSourceData importedData;
		Require(HE::DecodeMaterialArtifact(importResult.Artifact, importedData).Succeeded(), "Expected imported material decode");
		Require(
			std::get<std::string>(importedData.Parameters.at("u_Texture").Value) == textureGuid,
			"Expected material texture reference converted to guid");
	}

	void TestMaterialImportPipeline(const std::filesystem::path& root) {
		HE::ProjectService projectService;
		HE::ProjectContext context;
		Require(projectService.InitializeProject(root, &context, "MaterialImportProject").Succeeded(), "Expected material import project initialization");

		auto sourceMaterial = HE::Rendering::Material::Create("ImportedMaterial", HE::Rendering::MaterialType::Unlit);
		sourceMaterial->AddParameter({ "u_Color", HE::Rendering::MaterialParameterType::Vec4, glm::vec4(0.2f, 0.4f, 0.6f, 1.0f) });
		sourceMaterial->AddParameter({ "u_Roughness", HE::Rendering::MaterialParameterType::Float, 0.35f });
		const auto materialPath = context.GetAssetRootPath() / "Materials" / "ImportedMaterial.material";
		std::filesystem::create_directories(materialPath.parent_path());
		Require(HE::Serialization::SaveMaterial(*sourceMaterial, materialPath.generic_string()), "Expected material source save");

		HE::AssetService assetService;
		HE::AssetHandle materialHandle = 0;
		Require(assetService.LoadMaterialAsset(context, "Materials/ImportedMaterial.material", &materialHandle).Succeeded(), "Expected material source registration");

		HE::AssetImportReport report;
		Require(assetService.InitializeProjectAssets(context, &report).Succeeded(), "Expected material project asset initialization");
		Require(report.ImportedAssets == 7 && report.FailedAssets == 0, "Expected material and builtin artifact import");

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
		Require(firstReport.RegisteredAssets == 3, "Expected unregistered assets to be registered");
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

		const auto outsidePath = context.RootPath / "Outside.mesh";
		Require(HE::Rendering::Mesh::SaveToFile(*HE::Rendering::Mesh::CreateQuad("Outside"), outsidePath.generic_string()), "Expected outside mesh fixture");
		const auto manifestSize = assetService.GetManifest().Size();
		Require(assetService.ReimportAssets(context, outsidePath).Failed(), "Expected path outside Assets rejection");
		Require(assetService.GetManifest().Size() == manifestSize, "Expected outside path not to mutate manifest");
		Require(assetService.ReimportAssets(context, unsupportedPath).RequiresManualIntervention(), "Expected unsupported single file manual intervention");
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
	TestPngTextureImport(smokeRoot / "TextureSource");
	TestAssetReimportPipeline(smokeRoot / "ReimportProject");
	std::filesystem::remove_all(smokeRoot, errorCode);
	Require(!errorCode, "Expected import smoke cleanup after test");

	std::cout << "AssetImportSmoke passed" << std::endl;
	return 0;
}
