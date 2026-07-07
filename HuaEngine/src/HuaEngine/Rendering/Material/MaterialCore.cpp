#include "enginepch.h"
#include "Material.h"
#include "HuaEngine/Core/Log.h"

namespace HE::Rendering {

	// Material implementation
	Material::Material(const std::string& name, MaterialType type)
		: m_Name(name), m_Type(type), m_NextTextureSlot(0)
	{
	}

	Ref<Material> Material::Create(const std::string& name, MaterialType type)
	{
		return CreateRef<Material>(name, type);
	}

	Ref<Material> Material::CreateFromDeserialization()
	{
		return CreateRef<Material>("", MaterialType::Custom);
	}

	void Material::AddParameter(const MaterialParameter& parameter)
	{
		m_Parameters[parameter.Name] = parameter;

		// If texture parameter, auto-assign texture slot
		if (parameter.Type == MaterialParameterType::Texture2D)
		{
			SetTextureSlot(parameter.Name, m_NextTextureSlot++);
		}
	}

	bool Material::HasParameter(const std::string& name) const
	{
		return m_Parameters.find(name) != m_Parameters.end();
	}

	const MaterialParameter* Material::GetParameter(const std::string& name) const
	{
		auto it = m_Parameters.find(name);
		return (it != m_Parameters.end()) ? &it->second : nullptr;
	}

	void Material::SetTextureSlot(const std::string& name, uint32_t slot)
	{
		m_TextureSlots[name] = slot;
	}

	uint32_t Material::GetTextureSlot(const std::string& name) const
	{
		auto it = m_TextureSlots.find(name);
		return (it != m_TextureSlots.end()) ? it->second : 0;
	}

	void Material::SetParameter(const std::string& name, const MaterialParameterValue& value)
	{
		if (!HasParameter(name))
		{
			HE_CORE_WARN("Material parameter '{0}' not found in material '{1}'", name, m_Name);
			return;
		}

		m_Parameters[name].Value = value;
	}

	Ref<MaterialInstance> Material::CreateInstance()
	{
		return CreateRef<MaterialInstance>(shared_from_this());
	}

	// MaterialInstance implementation
	MaterialInstance::MaterialInstance(Ref<Material> baseMaterial)
		: m_BaseMaterial(baseMaterial)
	{
		HE_CORE_ASSERT(baseMaterial, "Base material cannot be null");
	}

	void MaterialInstance::SetParameter(const std::string& name, const MaterialParameterValue& value)
	{
		if (!m_BaseMaterial)
		{
			HE_CORE_WARN("Cannot set material instance parameter '{0}' without a base material", name);
			return;
		}

		if (!m_BaseMaterial->HasParameter(name))
		{
			HE_CORE_WARN("Parameter '{0}' not found in base material", name);
			return;
		}

		m_ParameterOverrides[name] = *(m_BaseMaterial->GetParameter(name));
		m_ParameterOverrides[name].Value = value;
	}

	bool MaterialInstance::HasParameterOverride(const std::string& name) const
	{
		return m_ParameterOverrides.find(name) != m_ParameterOverrides.end();
	}

	const MaterialParameter* MaterialInstance::GetParameterOverride(const std::string& name) const
	{
		auto it = m_ParameterOverrides.find(name);
		return (it != m_ParameterOverrides.end()) ? &it->second : nullptr;
	}

}
