#pragma once

#include "MaterialCore.h"
#include "MaterialTypes.h"
#include <unordered_map>

namespace HE::Rendering {

	class MaterialLibrary {
	public:
		static MaterialLibrary& Instance();

		// 材质创建
		Ref<Material> CreateMaterial(const std::string& name, MaterialType type);
		Ref<StandardMaterial> CreateStandardMaterial(const std::string& name);
		Ref<UnlitMaterial> CreateUnlitMaterial(const std::string& name);
		Ref<CustomMaterial> CreateCustomMaterial(const std::string& name, Ref<Shader> shader);

		// 材质注册和获取
		void RegisterMaterial(const std::string& name, Ref<Material> material);
		Ref<Material> GetMaterial(const std::string& name);
		bool HasMaterial(const std::string& name) const;

		// 材质加载（后续实现文件加载）
		Ref<Material> LoadMaterial(const std::string& path);

		// 材质管理
		void RemoveMaterial(const std::string& name);
		void Clear();
		
		// 获取所有材质
		const std::unordered_map<std::string, Ref<Material>>& GetAllMaterials() const { return m_Materials; }

		// 默认材质
		void CreateDefaultMaterials();
		Ref<Material> GetDefaultMaterial() const { return m_DefaultMaterial; }

	private:
		MaterialLibrary() = default;
		~MaterialLibrary() = default;

		std::unordered_map<std::string, Ref<Material>> m_Materials;
		Ref<Material> m_DefaultMaterial;
	};

}
