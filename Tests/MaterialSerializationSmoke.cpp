#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <variant>

#include "HuaEngine.h"
#include "HuaEngine/Rendering/Material/MaterialSerializer.h"

namespace {
	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[MaterialSerializationSmoke] " << message << std::endl;
			std::exit(1);
		}
	}

	std::filesystem::path MakeSmokePath(const std::string& filename) {
		const auto root = std::filesystem::temp_directory_path() / "HuaEngineMaterialSerializationSmoke";
		std::error_code errorCode;
		std::filesystem::create_directories(root, errorCode);
		Require(!errorCode, "Expected material smoke temp directory creation to succeed");
		return root / filename;
	}
}

int main() {
	HE::Log::Init({ .EnableConsoleOutput = false });
	HE::Serialization::InitializeSerialization();

	auto standardMaterial = HE::Rendering::Material::Create("SmokeStandardMaterial", HE::Rendering::MaterialType::Standard);
	standardMaterial->AddParameter({ "u_BaseColor", HE::Rendering::MaterialParameterType::Vec4, glm::vec4(0.8f, 0.2f, 0.3f, 1.0f) });
	standardMaterial->AddParameter({ "u_Metallic", HE::Rendering::MaterialParameterType::Float, 0.7f });
	standardMaterial->AddParameter({ "u_Roughness", HE::Rendering::MaterialParameterType::Float, 0.3f });

	const auto materialPath = MakeSmokePath("standard_material.material");
	Require(HE::Serialization::SaveMaterial(*standardMaterial, materialPath.generic_string()), "Expected standard material save to succeed");
	const std::string materialText = [&]() {
		std::ifstream materialFile(materialPath);
		Require(materialFile.is_open(), "Expected saved material file to be readable");
		return std::string((std::istreambuf_iterator<char>(materialFile)), std::istreambuf_iterator<char>());
	}();
	Require(materialText.find("name: SmokeStandardMaterial") != std::string::npos, "Expected default material save to use YAML mapping style");
	Require(materialText.find("\"name\"") == std::string::npos, "Expected default material save not to use JSON object syntax");

	HE::Rendering::Material loadedMaterial;
	Require(HE::Serialization::LoadMaterial(materialPath.generic_string(), loadedMaterial), "Expected standard material load to succeed");
	Require(loadedMaterial.GetName() == "SmokeStandardMaterial", "Expected material name to round-trip");
	Require(loadedMaterial.GetType() == HE::Rendering::MaterialType::Standard, "Expected material type to round-trip");
	Require(loadedMaterial.HasParameter("u_BaseColor"), "Expected material vec4 parameter to round-trip");
	Require(loadedMaterial.HasParameter("u_Metallic"), "Expected material float parameter to round-trip");

	HE::Rendering::MaterialLibrary::Instance().RegisterMaterial(standardMaterial->GetName(), standardMaterial);
	auto materialInstance = HE::CreateRef<HE::Rendering::MaterialInstance>(standardMaterial);
	materialInstance->SetParameter("u_BaseColor", glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
	materialInstance->SetParameter("u_Metallic", 1.0f);

	const auto instancePath = MakeSmokePath("standard_material_instance.material");
	Require(HE::Serialization::SaveMaterialInstance(*materialInstance, instancePath.generic_string()), "Expected material instance save to succeed");
	const std::string instanceText = [&]() {
		std::ifstream instanceFile(instancePath);
		Require(instanceFile.is_open(), "Expected saved material instance file to be readable");
		return std::string((std::istreambuf_iterator<char>(instanceFile)), std::istreambuf_iterator<char>());
	}();
	Require(instanceText.find("base_material_name: SmokeStandardMaterial") != std::string::npos, "Expected default material instance save to use YAML mapping style");
	Require(instanceText.find("\"base_material_name\"") == std::string::npos, "Expected default material instance save not to use JSON object syntax");

	HE::Rendering::MaterialInstance loadedInstance;
	Require(HE::Serialization::LoadMaterialInstance(instancePath.generic_string(), loadedInstance), "Expected material instance load to succeed");
	Require(loadedInstance.GetBaseMaterial() == standardMaterial, "Expected material instance base material to resolve from library");
	Require(loadedInstance.HasParameterOverride("u_BaseColor"), "Expected material instance color override to round-trip");
	Require(loadedInstance.HasParameterOverride("u_Metallic"), "Expected material instance metallic override to round-trip");

	auto nullTextureMaterial = HE::Rendering::Material::Create("NullTextureMaterial", HE::Rendering::MaterialType::Custom);
	nullTextureMaterial->AddParameter({ "u_NullTexture", HE::Rendering::MaterialParameterType::Texture2D, HE::Ref<HE::Rendering::TextureResource>() });
	nullTextureMaterial->SetParameter("u_NullTexture", HE::Ref<HE::Rendering::TextureResource>());

	const auto* nullTextureParameter = nullTextureMaterial->GetParameter("u_NullTexture");
	Require(nullTextureParameter != nullptr, "Expected null texture material parameter to remain stored");
	const auto* storedNullTexture = std::get_if<HE::Ref<HE::Rendering::TextureResource>>(&nullTextureParameter->Value);
	Require(storedNullTexture != nullptr && !*storedNullTexture, "Expected null texture parameter value to remain stored");

	std::error_code errorCode;
	std::filesystem::remove_all(materialPath.parent_path(), errorCode);
	Require(!errorCode, "Expected material smoke temp cleanup to succeed");

	std::cout << "MaterialSerializationSmoke passed" << std::endl;
	return 0;
}
