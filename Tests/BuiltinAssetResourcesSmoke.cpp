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
		std::filesystem::path("Meshes/Cube.obj"),
		std::filesystem::path("Meshes/Sphere.obj"),
		std::filesystem::path("Meshes/Fallback.obj"),
		std::filesystem::path("Materials/Default.material"),
		std::filesystem::path("Materials/Fallback.material"),
		std::filesystem::path("Shaders/UnlitColor.shader"),
		std::filesystem::path("Shaders/UnlitColor.hlsl")
	};

	for (const auto& relativePath : requiredFiles) {
		const auto assetPath = builtinRoot / relativePath;
		Require(
			std::filesystem::is_regular_file(assetPath),
			"Missing packaged builtin asset: " + assetPath.generic_string());
	}

	const auto resourceRoot = HE::ResourcePaths::GetEngineResourceRoot();
	const std::array removedDemoFiles = {
		std::filesystem::path("Cube.mesh"),
		std::filesystem::path("CustomMesh.mesh"),
		std::filesystem::path("Quad.mesh"),
		std::filesystem::path("Sphere.mesh"),
		std::filesystem::path("SandboxMaterial.material"),
		std::filesystem::path("SandboxScene.scene"),
		std::filesystem::path("shaders/sandbox.glsl")
	};

	for (const auto& relativePath : removedDemoFiles) {
		const auto assetPath = resourceRoot / relativePath;
		Require(
			!std::filesystem::exists(assetPath),
			"Legacy demo asset should not be packaged: " + assetPath.generic_string());
	}

	std::cout << "BuiltinAssetResourcesSmoke passed" << std::endl;
	return 0;
}
