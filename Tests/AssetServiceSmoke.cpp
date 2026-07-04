#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "HuaEngine.h"
#include "HuaEngine/Asset/AssetManifest.h"
#include "HuaEngine/Asset/AssetService.h"
#include "HuaEngine/Project/ProjectService.h"

namespace {
	class FakeTexture2D final : public HE::Rendering::Texture2D {
	public:
		uint32_t GetRenderID() const override { return 0; }
		uint32_t GetWidth() const override { return 1; }
		uint32_t GetHeight() const override { return 1; }
		void Bind(uint32_t slot = 0) override { (void)slot; }
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
	Require(meshFileText.find("\"name\"") != std::string::npos, "Expected mesh asset to persist a modern root name field");
	Require(meshFileText.find("\"vertex_data\"") != std::string::npos, "Expected mesh asset to persist vertex_data");
	Require(meshFileText.find("\"VertexData\"") == std::string::npos, "Expected mesh asset to avoid legacy reflected field names");

	const auto materialAssetPath = projectContext.GetAssetRootPath() / "Materials" / "SmokeMaterial.mat";
	std::filesystem::create_directories(materialAssetPath.parent_path(), errorCode);
	Require(!errorCode, "Expected material asset directory creation to succeed");

	const auto runtimeMaterial = HE::Rendering::Material::Create("SmokeMaterial", HE::Rendering::MaterialType::Unlit);
	Require(static_cast<bool>(runtimeMaterial), "Expected runtime material creation to succeed");
	Require(HE::Serialization::SaveMaterial(*runtimeMaterial, materialAssetPath.generic_string()), "Expected material asset file save to succeed");
	const auto materialFileText = ReadFileText(materialAssetPath);
	Require(materialFileText.find("\"material_type\"") != std::string::npos, "Expected material asset to persist material_type");
	Require(materialFileText.find("\"type\"") == std::string::npos, "Expected material asset to avoid generic type fields");

	const auto textureAssetPath = projectContext.GetAssetRootPath() / "Textures" / "SmokeTexture.txt";
	std::filesystem::create_directories(textureAssetPath.parent_path(), errorCode);
	Require(!errorCode, "Expected texture asset directory creation to succeed");

	std::ofstream texturePlaceholder(textureAssetPath);
	Require(texturePlaceholder.good(), "Expected placeholder texture file creation to succeed");
	texturePlaceholder << "placeholder texture payload";
	texturePlaceholder.close();

	const auto manifestPath = projectContext.RootPath / ".hua" / "assets.json";
	HE::AssetManifest manifest;
	auto initManifestResult = HE::LoadOrCreateAssetManifest(projectContext, manifest);
	Require(initManifestResult.Succeeded(), "Expected asset manifest to initialize");
	Require(std::filesystem::is_regular_file(manifestPath), "Expected .hua/assets.json to be created");
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
	WriteFileText(manifestPath, originalManifestText);

	HE::AssetService assetService;

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

	HE::Ref<HE::Texture2D> unresolvedTexture;
	auto resolveTextureResult = assetService.ResolveTextureAsset(textureHandle, unresolvedTexture);
	Require(resolveTextureResult.RequiresManualIntervention(), "Expected metadata-only texture registration to require manual intervention for runtime resolve");

	auto missingTextureResult = assetService.RegisterTextureAsset(projectContext, "Textures/MissingTexture.txt");
	Require(missingTextureResult.RequiresManualIntervention(), "Expected missing texture source file to require manual intervention");

	HE::AssetHandle runtimeOnlyTextureHandle = 0;
	auto runtimeOnlyTextureResult = assetService.RegisterTextureAsset(projectContext, "Textures/RuntimeOnlyTexture.txt", HE::CreateRef<FakeTexture2D>(), &runtimeOnlyTextureHandle);
	Require(runtimeOnlyTextureResult.RequiresManualIntervention(), "Expected runtime-only texture registration to require manual intervention");
	Require(runtimeOnlyTextureHandle == 0, "Expected runtime-only texture registration to avoid assigning a handle");

	HE::AssetRecord missingRecord;
	auto missingAssetResult = assetService.ResolveAsset(static_cast<HE::AssetHandle>(9999), missingRecord);
	Require(missingAssetResult.Failed(), "Expected resolving an unknown asset handle to fail");

	Require(assetService.GetAssetRegistry().GetAssetCount() == 3, "Expected registry to contain exactly three operational asset records");

	std::filesystem::remove_all(smokeRoot, errorCode);
	Require(!errorCode, "Expected asset smoke temporary directory cleanup to succeed");

	std::cout << "AssetServiceSmoke passed" << std::endl;
	return 0;
}
