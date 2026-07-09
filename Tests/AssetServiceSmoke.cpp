#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "HuaEngine.h"
#include "HuaEngine/Asset/AssetManifest.h"
#include "HuaEngine/Asset/AssetResolver.h"
#include "HuaEngine/Asset/AssetService.h"
#include "HuaEngine/Application/ApplicationServices.h"
#include "HuaEngine/Project/ProjectService.h"

namespace {
	class FakeTextureResource final : public HE::Rendering::TextureResource {
	public:
		const HE::Rendering::TextureDesc& GetDesc() const override { return m_Desc; }
		uint32_t GetWidth() const override { return 1; }
		uint32_t GetHeight() const override { return 1; }

	private:
		HE::Rendering::TextureDesc m_Desc{ .SourcePath = "fake://asset-service-smoke" };
	};

	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[AssetServiceSmoke] " << message << std::endl;
			std::exit(1);
		}
	}

	void WriteFileText(const std::filesystem::path& path, const std::string& text) {
		std::filesystem::create_directories(path.parent_path());
		std::ofstream stream(path, std::ios::out | std::ios::binary | std::ios::trunc);
		Require(stream.good(), "Expected file write to succeed: " + path.generic_string());
		stream << text;
	}

	std::string ReadFileText(const std::filesystem::path& path) {
		std::ifstream stream(path, std::ios::in | std::ios::binary);
		Require(stream.good(), "Expected file read to succeed: " + path.generic_string());
		return std::string((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
	}

	HE::AssetManifestRecord MakeManifestRecord(
		const HE::AssetGuid& guid,
		std::string assetId,
		HE::AssetKind kind = HE::AssetKind::Mesh,
		HE::AssetSource source = HE::AssetSource::File) {
		HE::AssetManifestRecord record;
		record.Guid = guid;
		record.AssetId = std::move(assetId);
		record.Kind = kind;
		record.Source = source;
		record.RelativePath = record.AssetId;
		record.ImportState = source == HE::AssetSource::Builtin ? HE::AssetImportState::Builtin : HE::AssetImportState::Registered;
		return record;
	}

	HE::AssetRecord MakeRegistryRecord(const HE::AssetGuid& guid, std::string assetId) {
		HE::AssetRecord record;
		record.Guid = guid;
		record.AssetId = std::move(assetId);
		record.Kind = HE::AssetKind::Mesh;
		record.Source = HE::AssetSource::File;
		record.RelativePath = record.AssetId;
		record.ImportState = HE::AssetImportState::Registered;
		return record;
	}
}

int main() {
	HE::Log::Init();
	HE::Serialization::InitializeSerialization();

	const auto smokeRoot = std::filesystem::temp_directory_path() / "HuaEngineAssetServiceSmoke";
	std::error_code errorCode;
	std::filesystem::remove_all(smokeRoot, errorCode);

	HE::ProjectService projectService;
	HE::ProjectContext projectContext;
	auto initializeResult = projectService.InitializeProject(smokeRoot / "SmokeProject", &projectContext, "SmokeProject");
	Require(initializeResult.Succeeded(), "Expected project.initialize to succeed for asset smoke");

	const auto meshAssetPath = projectContext.GetAssetRootPath() / "Meshes" / "SmokeQuad.mesh";
	std::filesystem::create_directories(meshAssetPath.parent_path(), errorCode);
	Require(!errorCode, "Expected mesh asset directory creation to succeed");

	const auto runtimeMesh = HE::Mesh::CreateQuad("SmokeQuad");
	Require(static_cast<bool>(runtimeMesh), "Expected runtime mesh creation to succeed");
	Require(HE::Mesh::SaveToFile(*runtimeMesh, meshAssetPath.generic_string()), "Expected mesh asset file save to succeed");
	const auto meshFileText = ReadFileText(meshAssetPath);
	Require(meshFileText.find("name:") != std::string::npos, "Expected mesh asset to persist a YAML root name field");
	Require(meshFileText.find("vertex_data:") != std::string::npos, "Expected mesh asset to persist YAML vertex_data");
	Require(meshFileText.find("\"VertexData\"") == std::string::npos, "Expected mesh asset to avoid legacy reflected field names");

	const auto materialAssetPath = projectContext.GetAssetRootPath() / "Materials" / "SmokeMaterial.mat";
	std::filesystem::create_directories(materialAssetPath.parent_path(), errorCode);
	Require(!errorCode, "Expected material asset directory creation to succeed");

	const auto runtimeMaterial = HE::Rendering::Material::Create("SmokeMaterial", HE::Rendering::MaterialType::Unlit);
	Require(static_cast<bool>(runtimeMaterial), "Expected runtime material creation to succeed");
	Require(HE::Serialization::SaveMaterial(*runtimeMaterial, materialAssetPath.generic_string()), "Expected material asset file save to succeed");
	const auto materialFileText = ReadFileText(materialAssetPath);
	Require(materialFileText.find("material_type:") != std::string::npos, "Expected material asset to persist YAML material_type");
	Require(materialFileText.find("\"type\"") == std::string::npos, "Expected material asset to avoid generic type fields");

	const auto textureAssetPath = projectContext.GetAssetRootPath() / "Textures" / "SmokeTexture.txt";
	std::filesystem::create_directories(textureAssetPath.parent_path(), errorCode);
	Require(!errorCode, "Expected texture asset directory creation to succeed");

	std::ofstream texturePlaceholder(textureAssetPath);
	Require(texturePlaceholder.good(), "Expected placeholder texture file creation to succeed");
	texturePlaceholder << "placeholder texture payload";
	texturePlaceholder.close();

	const auto manifestPath = projectContext.RootPath / ".huaengine" / "assets.json";
	HE::AssetManifest manifest;
	auto initManifestResult = HE::LoadOrCreateAssetManifest(projectContext, manifest);
	Require(initManifestResult.Succeeded(), "Expected asset manifest to initialize");
	Require(std::filesystem::is_regular_file(manifestPath), "Expected .huaengine/assets.json to be created");
	Require(manifest.FindByGuid(HE::BuiltinAssetGuids::QuadMesh) != nullptr, "Expected builtin quad mesh GUID");
	Require(manifest.FindByGuid(HE::BuiltinAssetGuids::FallbackMaterial) != nullptr, "Expected builtin fallback material GUID");
	Require(manifest.FindByAssetId("builtin/mesh/quad") != nullptr, "Expected builtin quad asset id lookup");

	HE::AssetManifest upsertManifest;
	Require(upsertManifest.Upsert(MakeManifestRecord("guid-a", "Meshes/A.mesh")), "Expected manifest initial upsert to succeed");
	Require(upsertManifest.Upsert(MakeManifestRecord("guid-a", "Meshes/A.mesh", HE::AssetKind::Material)), "Expected manifest exact key update to succeed");
	Require(!upsertManifest.Upsert(MakeManifestRecord("guid-b", "Meshes/A.mesh")), "Expected manifest asset id replacement to be rejected");
	Require(!upsertManifest.Upsert(MakeManifestRecord("guid-a", "Meshes/B.mesh")), "Expected manifest guid replacement to be rejected");
	Require(upsertManifest.Upsert(MakeManifestRecord("guid-b", "Meshes/B.mesh")), "Expected second manifest record to succeed");
	Require(!upsertManifest.Upsert(MakeManifestRecord("guid-a", "Meshes/B.mesh")), "Expected manifest cross-index conflict to be rejected");
	Require(upsertManifest.FindByGuid("guid-a")->AssetId == "Meshes/A.mesh", "Expected manifest guid index to remain stable after rejected conflicts");
	Require(upsertManifest.FindByAssetId("Meshes/B.mesh")->Guid == "guid-b", "Expected manifest asset id index to remain stable after rejected conflicts");

	HE::AssetRegistry upsertRegistry;
	const auto registryHandleA = upsertRegistry.Upsert(MakeRegistryRecord("reg-guid-a", "Meshes/A.mesh"));
	Require(registryHandleA != 0, "Expected registry initial upsert to succeed");
	const auto registryHandleAUpdate = upsertRegistry.Upsert(MakeRegistryRecord("reg-guid-a", "Meshes/A.mesh"));
	Require(registryHandleAUpdate == registryHandleA, "Expected registry exact key update to preserve handle");
	Require(upsertRegistry.Upsert(MakeRegistryRecord("reg-guid-b", "Meshes/A.mesh")) == 0, "Expected registry asset id replacement to be rejected");
	Require(upsertRegistry.Upsert(MakeRegistryRecord("reg-guid-a", "Meshes/B.mesh")) == 0, "Expected registry guid replacement to be rejected");
	const auto registryHandleB = upsertRegistry.Upsert(MakeRegistryRecord("reg-guid-b", "Meshes/B.mesh"));
	Require(registryHandleB != 0, "Expected second registry record to succeed");
	Require(upsertRegistry.Upsert(MakeRegistryRecord("reg-guid-a", "Meshes/B.mesh")) == 0, "Expected registry cross-index conflict to be rejected");
	Require(upsertRegistry.FindByGuid("reg-guid-a")->AssetId == "Meshes/A.mesh", "Expected registry guid index to remain stable after rejected conflicts");
	Require(upsertRegistry.Find("Meshes/B.mesh")->Guid == "reg-guid-b", "Expected registry asset id index to remain stable after rejected conflicts");

	const auto originalManifestText = ReadFileText(manifestPath);
	WriteFileText(manifestPath, "{ \"version\": 1, \"assets\": [ { \"guid\": \"bad-guid\" } ] }");
	HE::AssetManifest badManifest;
	auto badManifestResult = HE::LoadOrCreateAssetManifest(projectContext, badManifest);
	Require(badManifestResult.Failed(), "Expected malformed manifest to fail instead of being overwritten");
	Require(ReadFileText(manifestPath).find("bad-guid") != std::string::npos, "Expected malformed manifest to remain on disk after failed load");

	WriteFileText(manifestPath,
		"{\n"
		"  \"version\": 1,\n"
		"  \"assets\": [\n"
		"    { \"guid\": \"dup-guid\", \"asset_id\": \"Meshes/A.mesh\", \"kind\": \"mesh\", \"source\": \"file\", \"relative_path\": \"Meshes/A.mesh\", \"builtin_name\": \"\", \"import_state\": \"registered\" },\n"
		"    { \"guid\": \"dup-guid\", \"asset_id\": \"Meshes/B.mesh\", \"kind\": \"mesh\", \"source\": \"file\", \"relative_path\": \"Meshes/B.mesh\", \"builtin_name\": \"\", \"import_state\": \"registered\" }\n"
		"  ]\n"
		"}\n");
	Require(HE::LoadAssetManifest(projectContext, badManifest).Failed(), "Expected duplicate manifest GUID to fail load");

	WriteFileText(manifestPath,
		"{\n"
		"  \"version\": 1,\n"
		"  \"assets\": [\n"
		"    { \"guid\": \"guid-a\", \"asset_id\": \"Meshes/A.mesh\", \"kind\": \"mesh\", \"source\": \"file\", \"relative_path\": \"Meshes/A.mesh\", \"builtin_name\": \"\", \"import_state\": \"registered\" },\n"
		"    { \"guid\": \"guid-b\", \"asset_id\": \"Meshes/A.mesh\", \"kind\": \"mesh\", \"source\": \"file\", \"relative_path\": \"Meshes/A.mesh\", \"builtin_name\": \"\", \"import_state\": \"registered\" }\n"
		"  ]\n"
		"}\n");
	Require(HE::LoadAssetManifest(projectContext, badManifest).Failed(), "Expected duplicate manifest asset id to fail load");

	WriteFileText(manifestPath,
		"{\n"
		"  \"version\": 1,\n"
		"  \"assets\": [\n"
		"    { \"guid\": \"same-key\", \"asset_id\": \"Meshes/Same.mesh\", \"kind\": \"mesh\", \"source\": \"file\", \"relative_path\": \"Meshes/Same.mesh\", \"builtin_name\": \"first\", \"import_state\": \"registered\" },\n"
		"    { \"guid\": \"same-key\", \"asset_id\": \"Meshes/Same.mesh\", \"kind\": \"material\", \"source\": \"builtin\", \"relative_path\": \"\", \"builtin_name\": \"second\", \"import_state\": \"builtin\" }\n"
		"  ]\n"
		"}\n");
	Require(HE::LoadAssetManifest(projectContext, badManifest).Failed(), "Expected exact duplicate manifest record keys with different metadata to fail load");
	Require(HE::LoadOrCreateAssetManifest(projectContext, badManifest).Failed(), "Expected exact duplicate manifest record keys to fail init without overwrite");
	Require(ReadFileText(manifestPath).find("same-key") != std::string::npos, "Expected exact duplicate manifest file to remain on disk after failed init");

	WriteFileText(manifestPath,
		"{\n"
		"  \"version\": 1,\n"
		"  \"assets\": [\n"
		"    { \"guid\": \"bad-enum\", \"asset_id\": \"Meshes/A.mesh\", \"kind\": \"invalid\", \"source\": \"file\", \"relative_path\": \"Meshes/A.mesh\", \"builtin_name\": \"\", \"import_state\": \"registered\" }\n"
		"  ]\n"
		"}\n");
	Require(HE::LoadAssetManifest(projectContext, badManifest).Failed(), "Expected invalid manifest enum to fail load");

	WriteFileText(manifestPath,
		"{\n"
		"  \"version\": 1,\n"
		"  \"assets\": [\n"
		"    { \"guid\": \"bad-path\", \"asset_id\": \"Meshes/A.mesh\", \"kind\": \"mesh\", \"source\": \"file\", \"relative_path\": \"../A.mesh\", \"builtin_name\": \"\", \"import_state\": \"registered\" }\n"
		"  ]\n"
		"}\n");
	Require(HE::LoadAssetManifest(projectContext, badManifest).Failed(), "Expected escaping manifest relative path to fail load");

	WriteFileText(manifestPath,
		"{\n"
		"  \"version\": 1,\n"
		"  \"assets\": [\n"
		"    { \"guid\": \"bad-builtin\", \"asset_id\": \"builtin/mesh/bad\", \"kind\": \"mesh\", \"source\": \"builtin\", \"relative_path\": \"\", \"builtin_name\": \"not-a-builtin\", \"import_state\": \"builtin\" }\n"
		"  ]\n"
		"}\n");
	Require(HE::LoadAssetManifest(projectContext, badManifest).Failed(), "Expected illegal builtin manifest metadata to fail load");
	WriteFileText(manifestPath, originalManifestText);

	HE::AssetService assetService;
	auto serviceManifestResult = assetService.LoadOrCreateManifest(projectContext);
	Require(serviceManifestResult.Succeeded(), "Expected asset service manifest initialization to succeed");

	HE::AssetResolver resolver(assetService);
	HE::Ref<HE::Rendering::Mesh> builtinQuad;
	auto builtinQuadResult = resolver.ResolveMesh(HE::BuiltinAssetGuids::QuadMesh, builtinQuad);
	Require(builtinQuadResult.Succeeded(), "Expected builtin quad mesh resolve to succeed");
	Require(static_cast<bool>(builtinQuad), "Expected builtin quad runtime mesh");

	HE::Ref<HE::Rendering::Material> fallbackMaterial;
	auto fallbackMaterialResult = resolver.ResolveMaterial(HE::BuiltinAssetGuids::FallbackMaterial, fallbackMaterial);
	Require(fallbackMaterialResult.Succeeded(), "Expected fallback material resolve to succeed");
	Require(static_cast<bool>(fallbackMaterial), "Expected fallback material runtime object");
	Require(HE::Rendering::MaterialLibrary::Instance().HasMaterial("builtin/material/fallback"), "Expected fallback material to be registered under its builtin asset id");

	HE::Ref<HE::Rendering::Material> defaultMaterial;
	auto defaultMaterialResult = resolver.ResolveMaterial(HE::BuiltinAssetGuids::DefaultMaterial, defaultMaterial);
	Require(defaultMaterialResult.Succeeded(), "Expected default material resolve to succeed");
	Require(static_cast<bool>(defaultMaterial), "Expected default material runtime object");
	Require(defaultMaterial != fallbackMaterial, "Expected fallback material to be distinct from default material");

	HE::AssetHandle meshHandle = 0;
	auto loadMeshResult = assetService.LoadMeshAsset(projectContext, "Meshes/SmokeQuad.mesh", &meshHandle);
	Require(loadMeshResult.Succeeded(), "Expected asset.load_mesh to succeed");
	Require(meshHandle != 0, "Expected mesh handle to be assigned");

	HE::Ref<HE::Mesh> loadedMesh;
	auto resolveMeshResult = assetService.ResolveMeshAsset(meshHandle, loadedMesh);
	Require(resolveMeshResult.Succeeded(), "Expected asset.resolve_mesh to succeed");
	Require(static_cast<bool>(loadedMesh), "Expected resolved mesh payload to be valid");
	Require(HE::MeshManager::Instance().GetMesh("Meshes/SmokeQuad.mesh") == loadedMesh, "Expected mesh manager to expose the registered asset id");

	HE::AssetHandle stableMeshHandle = 0;
	auto stableRegisterResult = assetService.RegisterMeshAsset(projectContext, "Meshes/SmokeQuad.mesh", loadedMesh, &stableMeshHandle);
	Require(stableRegisterResult.Succeeded(), "Expected repeated mesh registration to succeed");
	Require(stableMeshHandle == meshHandle, "Expected repeated mesh registration to preserve the original asset handle");

	HE::AssetRecord quadRecord;
	Require(assetService.ResolveAsset("Meshes/SmokeQuad.mesh", quadRecord).Succeeded(), "Expected asset record lookup by id");
	Require(!quadRecord.Guid.empty(), "Expected asset record to have stable guid");
	Require(quadRecord.Handle != 0, "Expected runtime handle to remain available");
	Require(quadRecord.Kind == HE::AssetKind::Mesh, "Expected mesh asset kind");

	HE::Ref<HE::Rendering::Mesh> resolvedByGuidA;
	HE::Ref<HE::Rendering::Mesh> resolvedByGuidB;
	Require(resolver.ResolveMesh(quadRecord.Guid, resolvedByGuidA).Succeeded(), "Expected mesh resolve by guid");
	Require(resolver.ResolveMesh(quadRecord.Guid, resolvedByGuidB).Succeeded(), "Expected second mesh resolve by guid");
	Require(resolvedByGuidA == resolvedByGuidB, "Expected resolver to reuse runtime cache");

	HE::AssetRecord staleKindRecord = quadRecord;
	staleKindRecord.Kind = HE::AssetKind::Material;
	staleKindRecord.Source = HE::AssetSource::Builtin;
	staleKindRecord.BuiltinName = "default";
	staleKindRecord.ImportState = HE::AssetImportState::Builtin;
	Require(assetService.GetAssetRegistry().Upsert(staleKindRecord) == quadRecord.Handle, "Expected stale kind metadata update to preserve handle");
	HE::Ref<HE::Rendering::Mesh> staleKindMesh;
	auto staleKindMeshResult = resolver.ResolveMesh(quadRecord.Guid, staleKindMesh);
	Require(staleKindMeshResult.Failed(), "Expected cached mesh resolve to fail when metadata kind changes");
	Require(!staleKindMesh, "Expected stale kind mismatch to avoid returning cached mesh");

	HE::AssetHandle materialHandle = 0;
	auto loadMaterialResult = assetService.LoadMaterialAsset(projectContext, "Materials/SmokeMaterial.mat", &materialHandle);
	Require(loadMaterialResult.Succeeded(), "Expected asset.load_material to succeed");
	Require(materialHandle != 0, "Expected material handle to be assigned");

	HE::Ref<HE::Material> loadedMaterial;
	auto resolveMaterialResult = assetService.ResolveMaterialAsset(materialHandle, loadedMaterial);
	Require(resolveMaterialResult.Succeeded(), "Expected asset.resolve_material to succeed");
	Require(static_cast<bool>(loadedMaterial), "Expected resolved material payload to be valid");
	Require(HE::Rendering::MaterialLibrary::Instance().GetMaterial("Materials/SmokeMaterial.mat") == loadedMaterial, "Expected material library to expose the registered asset id");

	HE::AssetHandle textureHandle = 0;
	auto registerTextureResult = assetService.RegisterTextureAsset(projectContext, "Textures/SmokeTexture.txt", nullptr, &textureHandle);
	Require(registerTextureResult.Succeeded(), "Expected asset.register_texture to succeed when source file exists");
	Require(textureHandle != 0, "Expected texture handle to be assigned");

	HE::AssetRecord textureRecord;
	auto resolveTextureRecordResult = assetService.ResolveAsset("Textures/SmokeTexture.txt", textureRecord);
	Require(resolveTextureRecordResult.Succeeded(), "Expected asset.resolve to succeed by asset id");
	Require(textureRecord.Handle == textureHandle, "Expected texture asset id lookup to resolve the original handle");
	Require(textureRecord.Kind == HE::AssetKind::Texture2D, "Expected texture asset kind to be Texture2D");
	Require(textureRecord.ExistsOnDisk, "Expected texture asset to report source file presence");

	HE::Ref<HE::TextureResource> unresolvedTexture;
	auto resolveTextureResult = assetService.ResolveTextureAsset(textureHandle, unresolvedTexture);
	Require(resolveTextureResult.RequiresManualIntervention(), "Expected metadata-only texture registration to require manual intervention for runtime resolve");
	Require(!resolveTextureResult.Details.empty(), "Expected source-only texture resolve to include diagnostics");
	Require(resolveTextureResult.Details.front().Code == "asset.texture.loader_unsupported", "Expected source-only texture resolve to report unsupported loader");

	assetService.GetRuntimeCache().StoreTexture(textureRecord.Guid, HE::CreateRef<FakeTextureResource>());
	HE::Ref<HE::TextureResource> cachedTexture;
	auto cachedTextureResult = resolver.ResolveTexture(textureRecord.Guid, cachedTexture);
	Require(cachedTextureResult.Succeeded(), "Expected runtime cached texture resolve to succeed");
	Require(static_cast<bool>(cachedTexture), "Expected cached texture runtime object");

	HE::AssetService builtinTextureAssetService;
	Require(builtinTextureAssetService.LoadOrCreateManifest(projectContext).Succeeded(), "Expected builtin texture asset service manifest initialization to succeed");
	HE::AssetRecord builtinTextureRecord;
	builtinTextureRecord.Guid = "builtin-texture-unsupported";
	builtinTextureRecord.AssetId = "builtin/texture/unsupported";
	builtinTextureRecord.Kind = HE::AssetKind::Texture2D;
	builtinTextureRecord.Source = HE::AssetSource::Builtin;
	builtinTextureRecord.BuiltinName = "unsupported";
	builtinTextureRecord.ImportState = HE::AssetImportState::Builtin;
	Require(builtinTextureAssetService.GetAssetRegistry().Upsert(builtinTextureRecord) != 0, "Expected builtin texture metadata seed to succeed");
	HE::AssetResolver builtinTextureResolver(builtinTextureAssetService);
	HE::Ref<HE::TextureResource> unsupportedBuiltinTexture;
	auto unsupportedBuiltinTextureResult = builtinTextureResolver.ResolveTexture(builtinTextureRecord.Guid, unsupportedBuiltinTexture);
	Require(unsupportedBuiltinTextureResult.RequiresManualIntervention(), "Expected unsupported builtin texture resolve to require manual intervention");
	Require(!unsupportedBuiltinTextureResult.Details.empty(), "Expected unsupported builtin texture resolve to include diagnostics");
	Require(unsupportedBuiltinTextureResult.Details.front().Code == "asset.texture.builtin_unsupported", "Expected unsupported builtin texture diagnostic code");

	auto missingTextureResult = assetService.RegisterTextureAsset(projectContext, "Textures/MissingTexture.txt");
	Require(missingTextureResult.RequiresManualIntervention(), "Expected missing texture source file to require manual intervention");

	HE::AssetHandle runtimeOnlyTextureHandle = 0;
	auto runtimeOnlyTextureResult = assetService.RegisterTextureAsset(projectContext, "Textures/RuntimeOnlyTexture.txt", HE::CreateRef<FakeTextureResource>(), &runtimeOnlyTextureHandle);
	Require(runtimeOnlyTextureResult.RequiresManualIntervention(), "Expected runtime-only texture registration to require manual intervention");
	Require(runtimeOnlyTextureHandle == 0, "Expected runtime-only texture registration to avoid assigning a handle");

	HE::AssetRecord missingCachedMeshRecord;
	missingCachedMeshRecord.Guid = "missing-cached-smoke-mesh";
	missingCachedMeshRecord.Kind = HE::AssetKind::Mesh;
	missingCachedMeshRecord.Source = HE::AssetSource::File;
	missingCachedMeshRecord.AssetId = "Meshes/MissingCached.mesh";
	missingCachedMeshRecord.RelativePath = std::filesystem::path("Meshes/MissingCached.mesh");
	missingCachedMeshRecord.AbsolutePath = projectContext.GetAssetRootPath() / missingCachedMeshRecord.RelativePath;
	missingCachedMeshRecord.ImportState = HE::AssetImportState::Registered;
	missingCachedMeshRecord.ExistsOnDisk = false;
	Require(assetService.GetAssetRegistry().Upsert(missingCachedMeshRecord) != 0, "Expected missing cached mesh metadata insertion to succeed");
	assetService.GetRuntimeCache().StoreMesh(missingCachedMeshRecord.Guid, loadedMesh);

	HE::AssetValidationReport missingCachedMeshValidationReport;
	auto missingCachedMeshValidation = assetService.ValidateRegistry(projectContext, &missingCachedMeshValidationReport);
	Require(missingCachedMeshValidation.RequiresManualIntervention(), "Expected missing cached mesh validation to require manual intervention");
	Require(missingCachedMeshValidationReport.MissingFileAssets == 1, "Expected missing cached mesh validation to count the missing file");
	Require(missingCachedMeshValidationReport.MetadataIssueCount() == 1, "Expected missing cached mesh validation to report one metadata issue");
	Require(missingCachedMeshValidationReport.RuntimeIssueCount() == 0, "Expected missing cached mesh validation to skip runtime issue counting");
	Require(missingCachedMeshValidation.Payload.at("metadata_issue_count") == "1", "Expected missing cached mesh metadata issue payload");
	Require(missingCachedMeshValidation.Payload.at("runtime_issue_count") == "0", "Expected missing cached mesh runtime issue payload");
	Require(missingCachedMeshValidation.Payload.at("fallback_asset_count") == "2", "Expected fallback asset payload to remain stable");

	HE::AssetService badBuiltinAssetService;
	Require(badBuiltinAssetService.LoadOrCreateManifest(projectContext).Succeeded(), "Expected bad-builtin asset service manifest initialization to succeed");
	HE::AssetRecord badBuiltinMeshRecord;
	badBuiltinMeshRecord.Guid = "bad-builtin-validation-mesh";
	badBuiltinMeshRecord.Kind = HE::AssetKind::Mesh;
	badBuiltinMeshRecord.Source = HE::AssetSource::Builtin;
	badBuiltinMeshRecord.AssetId = "builtin/mesh/bad-validation";
	badBuiltinMeshRecord.BuiltinName = "not-a-builtin";
	badBuiltinMeshRecord.ImportState = HE::AssetImportState::Builtin;
	Require(badBuiltinAssetService.GetAssetRegistry().Upsert(badBuiltinMeshRecord) != 0, "Expected bad builtin mesh metadata insertion to succeed");
	HE::AssetValidationReport badBuiltinValidationReport;
	auto badBuiltinValidation = badBuiltinAssetService.ValidateRegistry(projectContext, &badBuiltinValidationReport);
	Require(badBuiltinValidation.RequiresManualIntervention(), "Expected illegal builtin metadata validation to require manual intervention");
	Require(badBuiltinValidationReport.BuiltinMetadataIssues == 1, "Expected illegal builtin metadata validation to count the bad builtin");
	Require(badBuiltinValidationReport.MetadataIssueCount() == 1, "Expected illegal builtin metadata validation to report one metadata issue");
	Require(badBuiltinValidationReport.RuntimeIssueCount() == 0, "Expected illegal builtin metadata validation to skip runtime issue counting");
	Require(badBuiltinValidation.Payload.at("metadata_issue_count") == "1", "Expected illegal builtin metadata issue payload");
	Require(badBuiltinValidation.Payload.at("runtime_issue_count") == "0", "Expected illegal builtin runtime issue payload");

	HE::AssetRecord missingRecord;
	auto missingAssetResult = assetService.ResolveAsset(static_cast<HE::AssetHandle>(9999), missingRecord);
	Require(missingAssetResult.Failed(), "Expected resolving an unknown asset handle to fail");

	WriteFileText(manifestPath, originalManifestText);
	auto reloadManifestResult = assetService.LoadOrCreateManifest(projectContext);
	Require(reloadManifestResult.Succeeded(), "Expected asset service manifest reload to succeed");
	HE::Ref<HE::Rendering::Mesh> staleMesh;
	Require(resolver.ResolveMesh(quadRecord.Guid, staleMesh).Failed(), "Expected manifest reload to remove stale mesh metadata and runtime cache");
	Require(assetService.GetAssetRegistry().GetAssetCount() == 6, "Expected registry reload to contain only manifest builtin records");

	HE::ApplicationServices applicationServices;
	Require(applicationServices.Assets().LoadOrCreateManifest(projectContext).Succeeded(), "Expected application asset service to load manifest before local resolver construction");
	HE::AssetResolver applicationResolver(applicationServices.Assets());
	HE::Ref<HE::Rendering::Mesh> applicationBuiltinQuad;
	Require(applicationResolver.ResolveMesh(HE::BuiltinAssetGuids::QuadMesh, applicationBuiltinQuad).Succeeded(), "Expected local application resolver to use loaded manifest metadata");

	std::filesystem::remove_all(smokeRoot, errorCode);
	Require(!errorCode, "Expected asset smoke temporary directory cleanup to succeed");

	std::cout << "AssetServiceSmoke passed" << std::endl;
	return 0;
}
