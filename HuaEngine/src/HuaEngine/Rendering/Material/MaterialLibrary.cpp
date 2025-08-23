#include "enginepch.h"
#include "MaterialLibrary.h"
#include "HuaEngine/Core/Log.h"

namespace HE {

	MaterialLibrary& MaterialLibrary::Instance()
	{
		static MaterialLibrary instance;
		return instance;
	}

	Ref<Material> MaterialLibrary::CreateMaterial(const std::string& name, MaterialType type)
	{
		switch (type)
		{
		case MaterialType::Standard:
			return CreateStandardMaterial(name);
		case MaterialType::Unlit:
			return CreateUnlitMaterial(name);
		case MaterialType::Custom:
			HE_CORE_WARN("Custom material requires a shader. Use CreateCustomMaterial instead.");
			return nullptr;
		default:
			HE_CORE_ERROR("Unknown material type");
			return nullptr;
		}
	}

	Ref<StandardMaterial> MaterialLibrary::CreateStandardMaterial(const std::string& name)
	{
		if (HasMaterial(name))
		{
			HE_CORE_WARN("Material '{0}' already exists", name);
			return std::dynamic_pointer_cast<StandardMaterial>(GetMaterial(name));
		}

		auto material = StandardMaterial::Create(name);
		RegisterMaterial(name, material);
		return material;
	}

	Ref<UnlitMaterial> MaterialLibrary::CreateUnlitMaterial(const std::string& name)
	{
		if (HasMaterial(name))
		{
			HE_CORE_WARN("Material '{0}' already exists", name);
			return std::dynamic_pointer_cast<UnlitMaterial>(GetMaterial(name));
		}

		auto material = UnlitMaterial::Create(name);
		RegisterMaterial(name, material);
		return material;
	}

	Ref<CustomMaterial> MaterialLibrary::CreateCustomMaterial(const std::string& name, Ref<Shader> shader)
	{
		if (HasMaterial(name))
		{
			HE_CORE_WARN("Material '{0}' already exists", name);
			return std::dynamic_pointer_cast<CustomMaterial>(GetMaterial(name));
		}

		auto material = CustomMaterial::Create(name, shader);
		RegisterMaterial(name, material);
		return material;
	}

	void MaterialLibrary::RegisterMaterial(const std::string& name, Ref<Material> material)
	{
		if (HasMaterial(name))
		{
			HE_CORE_WARN("Overwriting existing material '{0}'", name);
		}

		m_Materials[name] = material;
		HE_CORE_INFO("Registered material '{0}'", name);
	}

	Ref<Material> MaterialLibrary::GetMaterial(const std::string& name)
	{
		auto it = m_Materials.find(name);
		if (it != m_Materials.end())
		{
			return it->second;
		}

		HE_CORE_WARN("Material '{0}' not found, returning default material", name);
		return m_DefaultMaterial;
	}

	bool MaterialLibrary::HasMaterial(const std::string& name) const
	{
		return m_Materials.find(name) != m_Materials.end();
	}

	Ref<Material> MaterialLibrary::LoadMaterial(const std::string& path)
	{
		// TODO: 实现从文件加载材质
		HE_CORE_WARN("Material loading from file not yet implemented: {0}", path);
		return m_DefaultMaterial;
	}

	void MaterialLibrary::RemoveMaterial(const std::string& name)
	{
		auto it = m_Materials.find(name);
		if (it != m_Materials.end())
		{
			m_Materials.erase(it);
			HE_CORE_INFO("Removed material '{0}'", name);
		}
		else
		{
			HE_CORE_WARN("Cannot remove material '{0}': not found", name);
		}
	}

	void MaterialLibrary::Clear()
	{
		m_Materials.clear();
		m_DefaultMaterial = nullptr;
		HE_CORE_INFO("Cleared all materials from library");
	}

	void MaterialLibrary::CreateDefaultMaterials()
	{
		// 创建默认的 Unlit 材质
		m_DefaultMaterial = CreateUnlitMaterial("DefaultMaterial");
		
		// 设置默认参数
		auto defaultUnlit = std::dynamic_pointer_cast<UnlitMaterial>(m_DefaultMaterial);
		if (defaultUnlit)
		{
			defaultUnlit->SetColor(glm::vec4(1.0f, 0.0f, 1.0f, 1.0f)); // 洋红色，表示缺失纹理
		}

		HE_CORE_INFO("Created default materials");
	}

}
