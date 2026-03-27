#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "HuaEngine.h"
#include "HuaEngine/Asset/AssetService.h"
#include "HuaEngine/Project/ProjectService.h"

namespace {
	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[AssetServiceSmoke] " << message << std::endl;
			std::exit(1);
		}
	}

	std::string ReadFileText(const std::filesystem::path& path) {
		std::ifstream stream(path, std::ios::in | std::ios::binary);
		Require(stream.good(), "Expected file read to succeed: " + path.generic_string());
		return std::string((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
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

	HE::AssetRecord missingRecord;
	auto missingAssetResult = assetService.ResolveAsset(static_cast<HE::AssetHandle>(9999), missingRecord);
	Require(missingAssetResult.Failed(), "Expected resolving an unknown asset handle to fail");

	Require(assetService.GetRegistry().GetAssetCount() == 3, "Expected registry to contain exactly three operational asset records");

	std::filesystem::remove_all(smokeRoot, errorCode);
	Require(!errorCode, "Expected asset smoke temporary directory cleanup to succeed");

	std::cout << "AssetServiceSmoke passed" << std::endl;
	return 0;
}
