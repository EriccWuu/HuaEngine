#include "enginepch.h"
#include "MaterialTypes.h"
#include "HuaEngine/Core/Log.h"

namespace HE::Rendering {

	// StandardMaterial 实现
	StandardMaterial::StandardMaterial(const std::string& name)
		: Material(name, MaterialType::Standard)
	{
		InitializeParameters();
	}

	void StandardMaterial::InitializeParameters()
	{
		// 基础颜色参数
		AddParameter(MaterialParameter("u_BaseColor", MaterialParameterType::Vec4, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)));
		
		// PBR 参数
		AddParameter(MaterialParameter("u_Metallic", MaterialParameterType::Float, 0.0f));
		AddParameter(MaterialParameter("u_Roughness", MaterialParameterType::Float, 0.5f));
		AddParameter(MaterialParameter("u_AO", MaterialParameterType::Float, 1.0f));

		// 纹理参数
		AddParameter(MaterialParameter("u_DiffuseTexture", MaterialParameterType::Texture2D, Ref<Texture2D>()));
		AddParameter(MaterialParameter("u_NormalTexture", MaterialParameterType::Texture2D, Ref<Texture2D>()));
		AddParameter(MaterialParameter("u_MetallicRoughnessTexture", MaterialParameterType::Texture2D, Ref<Texture2D>()));
		AddParameter(MaterialParameter("u_AOTexture", MaterialParameterType::Texture2D, Ref<Texture2D>()));
	}

	void StandardMaterial::SetBaseColor(const glm::vec4& color)
	{
		SetParameter("u_BaseColor", color);
	}

	void StandardMaterial::SetMetallic(float metallic)
	{
		SetParameter("u_Metallic", metallic);
	}

	void StandardMaterial::SetRoughness(float roughness)
	{
		SetParameter("u_Roughness", roughness);
	}

	void StandardMaterial::SetAO(float ao)
	{
		SetParameter("u_AO", ao);
	}

	void StandardMaterial::SetDiffuseTexture(Ref<Texture2D> texture)
	{
		SetParameter("u_DiffuseTexture", texture);
	}

	void StandardMaterial::SetNormalTexture(Ref<Texture2D> texture)
	{
		SetParameter("u_NormalTexture", texture);
	}

	void StandardMaterial::SetMetallicRoughnessTexture(Ref<Texture2D> texture)
	{
		SetParameter("u_MetallicRoughnessTexture", texture);
	}

	void StandardMaterial::SetAOTexture(Ref<Texture2D> texture)
	{
		SetParameter("u_AOTexture", texture);
	}

	Ref<StandardMaterial> StandardMaterial::Create(const std::string& name)
	{
		return std::make_shared<StandardMaterial>(name);
	}

	// UnlitMaterial 实现
	UnlitMaterial::UnlitMaterial(const std::string& name)
		: Material(name, MaterialType::Unlit)
	{
		InitializeParameters();
	}

	void UnlitMaterial::InitializeParameters()
	{
		// 基础颜色参数
		AddParameter(MaterialParameter("u_Color", MaterialParameterType::Vec4, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)));
		
		// 纹理参数
		AddParameter(MaterialParameter("u_Texture", MaterialParameterType::Texture2D, Ref<Texture2D>()));
	}

	void UnlitMaterial::SetColor(const glm::vec4& color)
	{
		SetParameter("u_Color", color);
	}

	void UnlitMaterial::SetDiffuseTexture(Ref<Texture2D> texture)
	{
		SetParameter("u_Texture", texture);
	}

	Ref<UnlitMaterial> UnlitMaterial::Create(const std::string& name)
	{
		return std::make_shared<UnlitMaterial>(name);
	}

	// CustomMaterial 实现
	CustomMaterial::CustomMaterial(const std::string& name, Ref<Shader> shader)
		: Material(name, MaterialType::Custom)
	{
		SetShader(shader);
	}

	Ref<CustomMaterial> CustomMaterial::Create(const std::string& name, Ref<Shader> shader)
	{
		return std::make_shared<CustomMaterial>(name, shader);
	}

}
