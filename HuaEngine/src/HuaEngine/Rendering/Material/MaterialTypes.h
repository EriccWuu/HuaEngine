#pragma once

#include "Material.h"

namespace HE {

	// 标准 PBR 材质
	class StandardMaterial : public Material {
	public:
		StandardMaterial(const std::string& name = "StandardMaterial");
		virtual ~StandardMaterial() = default;

		// 预定义的 PBR 参数设置方法
		void SetBaseColor(const glm::vec4& color);
		void SetMetallic(float metallic);
		void SetRoughness(float roughness);
		void SetAO(float ao);

		// 预定义的纹理设置方法
		void SetDiffuseTexture(Ref<Texture2D> texture);
		void SetNormalTexture(Ref<Texture2D> texture);
		void SetMetallicRoughnessTexture(Ref<Texture2D> texture);
		void SetAOTexture(Ref<Texture2D> texture);

		static Ref<StandardMaterial> Create(const std::string& name = "StandardMaterial");

	private:
		void InitializeParameters();
	};

	// 无光照材质
	class UnlitMaterial : public Material {
	public:
		UnlitMaterial(const std::string& name = "UnlitMaterial");
		virtual ~UnlitMaterial() = default;

		// 预定义的参数设置方法
		void SetColor(const glm::vec4& color);
		void SetDiffuseTexture(Ref<Texture2D> texture);

		static Ref<UnlitMaterial> Create(const std::string& name = "UnlitMaterial");

	private:
		void InitializeParameters();
	};

	// 自定义材质
	class CustomMaterial : public Material {
	public:
		CustomMaterial(const std::string& name, Ref<Shader> shader);
		virtual ~CustomMaterial() = default;

		static Ref<CustomMaterial> Create(const std::string& name, Ref<Shader> shader);
	};

}
