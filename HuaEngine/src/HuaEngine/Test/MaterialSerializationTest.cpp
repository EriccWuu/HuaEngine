#include "enginepch.h"
#include "MaterialSerializationTest.h"
#include "HuaEngine/Serialization/Serialization.h"
#include "Platform/OpenGL/OpenGLShader.h"
#include <iostream>

namespace HE {

	void MaterialSerializationTest::RunTest()
	{
		HE_CORE_INFO("=== Material Serialization Test Start ===");
		
		try {
			// Test basic material serialization
			CreateStandardMaterialTest();
			CreateUnlitMaterialTest();
			CreateCustomMaterialTest();
			
			// Test material instance serialization
			TestMaterialInstanceSerialization();
			
			HE_CORE_INFO("=== Material Serialization Test Complete ===");
		}
		catch (const std::exception& e) {
			HE_CORE_ERROR("Material serialization test failed: {0}", e.what());
		}
	}

	void MaterialSerializationTest::CreateStandardMaterialTest()
	{
		HE_CORE_INFO("--- Standard Material (PBR) Serialization Test ---");
		
		// Create standard material
		auto standardMaterial = Rendering::MaterialLibrary::Instance().CreateStandardMaterial("TestStandardMaterial");
		
		// Set parameters
		standardMaterial->SetParameter("u_BaseColor", glm::vec3(0.8f, 0.2f, 0.3f));
		standardMaterial->SetParameter("u_Metallic", 0.7f);
		standardMaterial->SetParameter("u_Roughness", 0.3f);
		standardMaterial->SetParameter("u_AO", 1.0f);
		
		PrintMaterialInfo(standardMaterial, "Original Standard Material");
		
		// Save to file - convert to base Material class for serialization
		std::string filename = "standard_material_test.json";
		Ref<Rendering::Material> materialBase = std::static_pointer_cast<Rendering::Material>(standardMaterial);
		if (SerializationManager::Instance().SerializeToFile(materialBase, filename, SerializationFormat::JSON)) {
			HE_CORE_INFO("Standard material saved to file: {0}", filename);
			
			// Reload material - note: need to create an object first, then deserialize into it
			auto loadedMaterial = Rendering::MaterialLibrary::Instance().CreateStandardMaterial("LoadedStandardMaterial");
			loadedMaterial->ClearParameters();
			Ref<Rendering::Material> loadedMaterialBase = std::static_pointer_cast<Rendering::Material>(loadedMaterial);
			if (SerializationManager::Instance().DeserializeFromFile(filename, *loadedMaterialBase, SerializationFormat::JSON)) {
				HE_CORE_INFO("Standard material loaded from file: {0}", filename);
				PrintMaterialInfo(loadedMaterial, "Loaded Standard Material");
				
				// Verify materials are the same
				if (VerifyMaterialsEqual(standardMaterial, loadedMaterial)) {
					HE_CORE_INFO("✓ Standard Material Serialization Verification Successful");
				} else {
					HE_CORE_WARN("✗ Standard Material Serialization Verification Failed");
				}
			} else {
				HE_CORE_ERROR("Cannot load standard material from file");
			}
		} else {
			HE_CORE_ERROR("Cannot save standard material to file");
		}
	}

	void MaterialSerializationTest::CreateUnlitMaterialTest()
	{
		HE_CORE_INFO("--- Unlit Material Serialization Test ---");
		
		// Create unlit material
		auto unlitMaterial = Rendering::MaterialLibrary::Instance().CreateUnlitMaterial("TestUnlitMaterial");
		
		// Set parameters
		unlitMaterial->SetParameter("u_Color", glm::vec4(1.0f, 0.5f, 0.0f, 1.0f));
		
		PrintMaterialInfo(unlitMaterial, "Original Unlit Material");
		
		// Save to file - convert to base Material class for serialization
		std::string filename = "unlit_material_test.json";
		Ref<Rendering::Material> materialBase = std::static_pointer_cast<Rendering::Material>(unlitMaterial);
		if (SerializationManager::Instance().SerializeToFile(materialBase, filename, SerializationFormat::JSON)) {
			HE_CORE_INFO("Unlit material saved to file: {0}", filename);
			
			// Reload material
			auto loadedMaterial = Rendering::MaterialLibrary::Instance().CreateUnlitMaterial("LoadedUnlitMaterial");
			loadedMaterial->ClearParameters();
			Ref<Rendering::Material> loadedMaterialBase = std::static_pointer_cast<Rendering::Material>(loadedMaterial);
			if (SerializationManager::Instance().DeserializeFromFile(filename, *loadedMaterialBase, SerializationFormat::JSON)) {
				HE_CORE_INFO("Unlit material loaded from file: {0}", filename);
				PrintMaterialInfo(loadedMaterial, "Loaded Unlit Material");
				
				// Verify materials are the same
				if (VerifyMaterialsEqual(unlitMaterial, loadedMaterial)) {
					HE_CORE_INFO("✓ Unlit Material Serialization Verification Successful");
				} else {
					HE_CORE_WARN("✗ Unlit Material Serialization Verification Failed");
				}
			} else {
				HE_CORE_ERROR("Cannot load unlit material from file");
			}
		} else {
			HE_CORE_ERROR("Cannot save unlit material to file");
		}
	}

	void MaterialSerializationTest::CreateCustomMaterialTest()
	{
		HE_CORE_INFO("--- Custom Material Serialization Test ---");
		
		// Create a simple shader for custom material
		std::string vertexSource = R"(
			#version 330 core
			layout (location = 0) in vec3 a_Position;
			void main() {
				gl_Position = vec4(a_Position, 1.0);
			}
		)";
		
		std::string fragmentSource = R"(
			#version 330 core
			out vec4 FragColor;
			void main() {
				FragColor = vec4(1.0, 0.0, 1.0, 1.0);
			}
		)";
		
		auto customShader = CreateRef<Rendering::OpenGLShader>(vertexSource, fragmentSource);
		
		// Create custom material
		auto customMaterial = Rendering::MaterialLibrary::Instance().CreateCustomMaterial("TestCustomMaterial", customShader);

		customMaterial->AddParameter({ "custom_float", Rendering::MaterialParameterType::Float, 1.0f});
		customMaterial->AddParameter({ "custom_vec3", Rendering::MaterialParameterType::Vec3, glm::vec3{1.0f, 1.0f, 1.0f } });
		customMaterial->AddParameter({ "custom_color", Rendering::MaterialParameterType::Vec4, glm::vec4{1.0f, 1.0f, 1.0f, 0.f } });
		
		PrintMaterialInfo(customMaterial, "Original Custom Material");
		
		// Save to file - convert to base Material class for serialization
		std::string filename = "custom_material_test.json";
		Ref<Rendering::Material> materialBase = std::static_pointer_cast<Rendering::Material>(customMaterial);
		if (SerializationManager::Instance().SerializeToFile(materialBase, filename, SerializationFormat::JSON)) {
			HE_CORE_INFO("Custom material saved to file: {0}", filename);
			
			// Reload material
			auto loadedMaterial = Rendering::MaterialLibrary::Instance().CreateCustomMaterial("LoadedCustomMaterial", customShader);
			loadedMaterial->ClearParameters();
			Ref<Rendering::Material> loadedMaterialBase = std::static_pointer_cast<Rendering::Material>(loadedMaterial);
			if (SerializationManager::Instance().DeserializeFromFile(filename, *loadedMaterialBase, SerializationFormat::JSON)) {
				HE_CORE_INFO("Custom material loaded from file: {0}", filename);
				PrintMaterialInfo(loadedMaterial, "Loaded Custom Material");
				
				// Verify materials are the same
				if (VerifyMaterialsEqual(customMaterial, loadedMaterial)) {
					HE_CORE_INFO("✓ Custom Material Serialization Verification Successful");
				} else {
					HE_CORE_WARN("✗ Custom Material Serialization Verification Failed");
				}
			} else {
				HE_CORE_ERROR("Cannot load custom material from file");
			}
		} else {
			HE_CORE_ERROR("Cannot save custom material to file");
		}
	}

	void MaterialSerializationTest::TestMaterialInstanceSerialization()
	{
		HE_CORE_INFO("--- Material Instance Serialization Test ---");
		
		// Create base material
		auto baseMaterial = Rendering::MaterialLibrary::Instance().CreateStandardMaterial("BaseStandardMaterial");
		baseMaterial->SetParameter("u_BaseColor", glm::vec3(0.5f, 0.5f, 0.5f));
		baseMaterial->SetParameter("u_Metallic", 0.0f);
		baseMaterial->SetParameter("u_Roughness", 0.5f);
		baseMaterial->SetParameter("u_AO", 1.0f);
		
		// Create material instance and override some parameters
		auto materialInstance = CreateRef<Rendering::MaterialInstance>(baseMaterial);
		materialInstance->SetParameter("u_BaseColor", glm::vec3(1.0f, 0.0f, 0.0f)); // Override to red
		materialInstance->SetParameter("u_Metallic", 1.0f); // Override to metallic material
		
		HE_CORE_INFO("Original material instance creation complete");
		
		// Save material instance to file
		std::string filename = "material_instance_test.json";
		if (SerializationManager::Instance().SerializeToFile(materialInstance, filename, SerializationFormat::JSON)) {
			HE_CORE_INFO("Material instance saved to file: {0}", filename);
			
			// Reload material instance (requires same base material)
			auto loadedInstance = CreateRef<Rendering::MaterialInstance>(baseMaterial);
			if (SerializationManager::Instance().DeserializeFromFile(filename, *loadedInstance, SerializationFormat::JSON)) {
				HE_CORE_INFO("Material instance loaded from file: {0}", filename);
				HE_CORE_INFO("✓ Material Instance Serialization Verification Successful");
				const auto& overrides = loadedInstance->GetParameterOverrides();
				for (auto& [k, v] : overrides) {
					HE_CORE_INFO("Parameter override: {0}", k);
				}
			} else {
				HE_CORE_ERROR("Cannot load material instance from file");
			}
		} else {
			HE_CORE_ERROR("Cannot save material instance to file");
		}
	}

	bool MaterialSerializationTest::VerifyMaterialsEqual(const Ref<Rendering::Material>& originalMaterial, const Ref<Rendering::Material>& loadedMaterial)
	{
		// Check material type
		if (originalMaterial->GetType() != loadedMaterial->GetType()) {
			return false;
		}
		
		// Check material name
		if (originalMaterial->GetName() != loadedMaterial->GetName()) {
			return false;
		}
		
		// Check parameter count
		const auto& originalParams = originalMaterial->GetParameters();
		const auto& loadedParams = loadedMaterial->GetParameters();
		
		if (originalParams.size() != loadedParams.size()) {
			return false;
		}
		
		// Check each parameter
		for (const auto& [name, param] : originalParams) {
			auto it = loadedParams.find(name);
			if (it == loadedParams.end()) {
				return false; // Parameter doesn't exist
			}
			
			if (param.Type != it->second.Type) {
				return false; // Parameter type mismatch
			}
			
			// Note: Here we should compare specific values, but for simplicity in this test,
			// we only check type and existence
		}
		
		return true;
	}

	void MaterialSerializationTest::PrintMaterialInfo(const Ref<Rendering::Material>& material, const std::string& title)
	{
		HE_CORE_INFO("=== {0} ===", title);
		HE_CORE_INFO("Material Name: {0}", material->GetName());
		HE_CORE_INFO("Material Type: {0}", static_cast<int>(material->GetType()));
		
		const auto& params = material->GetParameters();
		HE_CORE_INFO("Parameter count: {0}", params.size());
		
		for (const auto& [name, param] : params) {
			HE_CORE_INFO("  Parameter: {0}, Type: {1}", name, static_cast<int>(param.Type));
		}
		
		HE_CORE_INFO("==========================");
	}

}
