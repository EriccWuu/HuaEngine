#pragma once

#include "MaterialCore.h"
#include "MaterialTypes.h"
#include <unordered_map>

namespace HE::Rendering {

	class MaterialLibrary {
	public:
		static MaterialLibrary& Instance();

		// Material creation
		Ref<Material> CreateMaterial(const std::string& name, MaterialType type);
		Ref<StandardMaterial> CreateStandardMaterial(const std::string& name);
		Ref<UnlitMaterial> CreateUnlitMaterial(const std::string& name);
		Ref<CustomMaterial> CreateCustomMaterial(const std::string& name, Ref<Shader> shader);

		// Material registration and retrieval
		void RegisterMaterial(const std::string& name, Ref<Material> material);
		Ref<Material> GetMaterial(const std::string& name);
		bool HasMaterial(const std::string& name) const;

		// Material loading (file loading to be implemented)
		Ref<Material> LoadMaterial(const std::string& path);

		// Material management
		void RemoveMaterial(const std::string& name);
		void Clear();

		// Get all materials
		const std::unordered_map<std::string, Ref<Material>>& GetAllMaterials() const { return m_Materials; }

		// Default material
		void CreateDefaultMaterials();
		Ref<Material> GetDefaultMaterial() const { return m_DefaultMaterial; }

	private:
		MaterialLibrary() = default;
		~MaterialLibrary() = default;

		std::unordered_map<std::string, Ref<Material>> m_Materials;
		Ref<Material> m_DefaultMaterial;
	};

}
