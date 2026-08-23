#include <array>
#include <cstdlib>
#include <filesystem>
#include <iostream>

#include "HuaEngine/Core/ResourcePaths.h"

namespace {
	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[BuiltinAssetResourcesSmoke] " << message << std::endl;
			std::exit(1);
		}
	}
}

int main() {
	const auto builtinRoot = HE::ResourcePaths::GetEngineResourceRoot() / "BuiltinAssets";
	const std::array requiredFiles = {
		std::filesystem::path("manifest.json"),
		std::filesystem::path("Meshes/Quad.obj"),
		std::filesystem::path("Meshes/Cube.mesh"),
		std::filesystem::path("Meshes/Sphere.mesh"),
		std::filesystem::path("Meshes/Fallback.mesh"),
		std::filesystem::path("Materials/Default.material"),
		std::filesystem::path("Materials/Fallback.material"),
		std::filesystem::path("Shaders/UnlitColor.glsl")
	};

	for (const auto& relativePath : requiredFiles) {
		const auto assetPath = builtinRoot / relativePath;
		Require(
			std::filesystem::is_regular_file(assetPath),
			"Missing packaged builtin asset: " + assetPath.generic_string());
	}

	std::cout << "BuiltinAssetResourcesSmoke passed" << std::endl;
	return 0;
}
