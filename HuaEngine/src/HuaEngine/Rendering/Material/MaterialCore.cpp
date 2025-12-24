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

	void Material::Bind()
	{
		if (m_Shader)
		{
			m_Shader->Bind();
		}
	}

	void Material::Unbind()
	{
		if (m_Shader)
		{
			m_Shader->Unbind();
		}
	}

	void Material::SetParameter(const std::string& name, const MaterialParameterValue& value)
	{
		if (!HasParameter(name))
		{
			HE_CORE_WARN("Material parameter '{0}' not found in material '{1}'", name, m_Name);
			return;
		}

		m_Parameters[name].Value = value;
		ApplyParameter(name, value);
	}

	void Material::ApplyParameter(const std::string& name, const MaterialParameterValue& value)
	{
		if (!m_Shader)
		{
			HE_CORE_ERROR("No shader bound to material '{0}'", m_Name);
			return;
		}

		std::visit([&](auto&& val) {
			using T = std::decay_t<decltype(val)>;
			
			if constexpr (std::is_same_v<T, int>)
			{
				m_Shader->SetInt(name, val);
			}
			else if constexpr (std::is_same_v<T, float>)
			{
				m_Shader->SetFloat(name, val);
			}
			else if constexpr (std::is_same_v<T, glm::vec2>)
			{
				m_Shader->SetFloat2(name, val);
			}
			else if constexpr (std::is_same_v<T, glm::vec3>)
			{
				m_Shader->SetFloat3(name, val);
			}
			else if constexpr (std::is_same_v<T, glm::vec4>)
			{
				m_Shader->SetFloat4(name, val);
			}
			else if constexpr (std::is_same_v<T, glm::mat3>)
			{
				m_Shader->SetMat3(name, val);
			}
			else if constexpr (std::is_same_v<T, glm::mat4>)
			{
				m_Shader->SetMat4(name, val);
			}
			else if constexpr (std::is_same_v<T, Ref<Texture2D>>)
			{
				uint32_t slot = GetTextureSlot(name);
				val->Bind(slot);
				m_Shader->SetInt(name, static_cast<int>(slot));
			}
			else if constexpr (std::is_same_v<T, std::vector<int>>)
			{
				m_Shader->SetIntArray(name, const_cast<int*>(val.data()), static_cast<uint32_t>(val.size()));
			}
			else if constexpr (std::is_same_v<T, std::vector<float>>)
			{
				// Note: Shader class needs SetFloatArray method
				HE_CORE_WARN("Float array parameters not yet supported");
			}
		}, value);
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

	void MaterialInstance::Bind()
	{
		m_BaseMaterial->Bind();
		ApplyParameters();
	}

	void MaterialInstance::Unbind()
	{
		m_BaseMaterial->Unbind();
	}

	void MaterialInstance::ApplyParameters()
	{
		// First apply base material's default parameters
		for (const auto& [name, param] : m_BaseMaterial->GetParameters())
		{
			// If instance hasn't overridden this parameter, use default value
			if (!HasParameterOverride(name))
			{
				m_BaseMaterial->ApplyParameter(name, param.Value);
			}
		}

		// Then apply instance's parameter overrides
		for (const auto& [name, value] : m_ParameterOverrides)
		{
			m_BaseMaterial->ApplyParameter(name, value.Value);
		}
	}

}
