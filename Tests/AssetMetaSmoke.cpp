#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "HuaEngine/Asset/Metadata/AssetMeta.h"

namespace {
	void Require(bool condition, const char* message) {
		if (!condition) {
			std::cerr << "[AssetMetaSmoke] " << message << std::endl;
			std::exit(1);
		}
	}
}

int main() {
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
	std::cout << "AssetMetaSmoke passed" << std::endl;
	return 0;
}
