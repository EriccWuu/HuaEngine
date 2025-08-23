#pragma once

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Rendering/Material/Material.h"

namespace HE {

	/// @brief Material serialization test class
	/// Demonstrates how to use the material system's serialization features to save and load materials
	class MaterialSerializationTest {
	public:
		/// @brief Run material serialization test
		/// Creates different types of materials, saves them to files, then reloads them
		static void RunTest();

	private:
		/// @brief Create standard material test
		static void CreateStandardMaterialTest();

		/// @brief Create unlit material test
		static void CreateUnlitMaterialTest();

		/// @brief Create custom material test
		static void CreateCustomMaterialTest();

		/// @brief Test material instance serialization
		static void TestMaterialInstanceSerialization();

		/// @brief Verify that serialized materials are correct
		/// @param originalMaterial Original material
		/// @param loadedMaterial Material loaded from file
		/// @return Returns true if both materials are equal
		static bool VerifyMaterialsEqual(const Ref<Material>& originalMaterial, const Ref<Material>& loadedMaterial);

		/// @brief Print material information to console
		/// @param material Material to print
		/// @param title Title
		static void PrintMaterialInfo(const Ref<Material>& material, const std::string& title);
	};

}
