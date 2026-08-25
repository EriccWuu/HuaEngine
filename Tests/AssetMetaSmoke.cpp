#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "HuaEngine.h"
#include "HuaEngine/Asset/Metadata/AssetMeta.h"
#include "HuaEngine/Asset/AssetManifest.h"
#include "HuaEngine/Asset/AssetService.h"
#include "HuaEngine/Project/ProjectService.h"

namespace {
	void Require(bool condition, const char* message) {
		if (!condition) {
			std::cerr << "[AssetMetaSmoke] " << message << std::endl;
			std::exit(1);
		}
	}
}

int main() {
	HE::Log::Init();
	HE::Serialization::InitializeSerialization();
	const HE::AssetMeta meta{
		.Guid = "0123456789abcdef0123456789abcdef",
		.ImporterId = "texture.png",
		.SettingsVersion = 1,
		.Settings = { .Values = { { "max_size", "4096" }, { "color_space", "srgb" } } }
	};
	std::string encoded;
	Require(HE::EncodeAssetMeta(meta, encoded).Succeeded(), "Expected canonical metadata encoding");
	Require(encoded.find("color_space") < encoded.find("max_size"), "Expected stable settings key ordering");
	HE::AssetMeta decoded;
	Require(HE::DecodeAssetMeta(encoded, decoded).Succeeded() && decoded == meta, "Expected metadata round trip");
	std::string firstSettingsDigest;
	std::string secondSettingsDigest;
	Require(HE::ComputeAssetMetaSettingsDigest(meta.SettingsVersion, meta.Settings, firstSettingsDigest).Succeeded(), "Expected settings digest");
	auto reorderedSettings = meta.Settings;
	Require(HE::ComputeAssetMetaSettingsDigest(meta.SettingsVersion, reorderedSettings, secondSettingsDigest).Succeeded() && firstSettingsDigest == secondSettingsDigest, "Expected canonical settings digest");
	reorderedSettings.Values["max_size"] = "2048";
	Require(HE::ComputeAssetMetaSettingsDigest(meta.SettingsVersion, reorderedSettings, secondSettingsDigest).Succeeded() && firstSettingsDigest != secondSettingsDigest, "Expected settings changes to affect digest");
	Require(HE::DecodeAssetMeta("meta_version: 2\nguid: x\nimporter: mesh.obj\nsettings_version: 1\nsettings: {}\n", decoded).Failed(), "Expected future metadata version rejection");
	Require(HE::DecodeAssetMeta("meta_version: 1\nguid: x\nimporter: mesh.obj\nsettings_version: 1\nsettings: { scale: 1, scale: 2 }\n", decoded).Failed(), "Expected duplicate settings key rejection");

	const auto root = std::filesystem::temp_directory_path() / "HuaEngineAssetMetaSmoke";
	const auto source = root / "Texture.png";
	std::error_code errorCode;
	std::filesystem::remove_all(root, errorCode);
	Require(HE::SaveAssetMeta(source, meta).Succeeded(), "Expected atomic metadata save");
	HE::AssetMeta loaded;
	Require(HE::LoadAssetMeta(source, loaded).Succeeded() && loaded == meta, "Expected saved metadata load");
	std::filesystem::remove_all(root, errorCode);
	Require(!errorCode, "Expected temporary metadata cleanup");

	HE::ProjectService projects;
	HE::ProjectContext context;
	Require(projects.InitializeProject(root, &context, "MetaMigration").Succeeded(), "Expected migration project setup");
	const auto meshPath = context.GetAssetRootPath() / "Legacy.obj";
	{
		std::ofstream stream(meshPath);
		stream << "o Legacy\n";
	}
	HE::AssetManifest legacyManifest;
	Require(legacyManifest.Upsert({ .Guid = "legacy-guid", .AssetId = "Legacy.obj", .Kind = HE::AssetKind::Mesh, .Source = HE::AssetSource::File, .RelativePath = "Legacy.obj", .ImportState = HE::AssetImportState::Registered }), "Expected legacy manifest fixture");
	Require(HE::SaveAssetManifest(context, legacyManifest).Succeeded(), "Expected legacy manifest save");
	HE::AssetService assets;
	Require(assets.LoadOrCreateManifest(context).Succeeded(), "Expected sidecar migration");
	HE::AssetMeta migrated;
	Require(HE::LoadAssetMeta(meshPath, migrated).Succeeded(), "Expected migrated sidecar");
	Require(migrated.Guid == "legacy-guid" && migrated.ImporterId == "hua.mesh-obj", "Expected legacy GUID and importer identity preservation");
	Require(assets.GetManifest().FindByAssetId("Legacy.obj") != nullptr, "Expected derived manifest record");
	std::filesystem::remove_all(root, errorCode);
	Require(!errorCode, "Expected migration project cleanup");
	std::cout << "AssetMetaSmoke passed" << std::endl;
	return 0;
}
