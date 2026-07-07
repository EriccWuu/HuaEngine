#pragma once

#include "MaterialCore.h"

namespace HE::Rendering {

	// Standard PBR material
	class StandardMaterial : public Material {
	public:
		StandardMaterial(const std::string& name = "StandardMaterial");
		virtual ~StandardMaterial() = default;

		// Predefined PBR parameter setters
		void SetBaseColor(const glm::vec4& color);
		void SetMetallic(float metallic);
		void SetRoughness(float roughness);
		void SetAO(float ao);

		// Predefined texture setters
		void SetDiffuseTexture(Ref<TextureResource> texture);
		void SetNormalTexture(Ref<TextureResource> texture);
		void SetMetallicRoughnessTexture(Ref<TextureResource> texture);
		void SetAOTexture(Ref<TextureResource> texture);

		static Ref<StandardMaterial> Create(const std::string& name = "StandardMaterial");

	private:
		void InitializeParameters();
	};

	// Unlit material
	class UnlitMaterial : public Material {
	public:
		UnlitMaterial(const std::string& name = "UnlitMaterial");
		virtual ~UnlitMaterial() = default;

		// Predefined parameter setters
		void SetColor(const glm::vec4& color);
		void SetDiffuseTexture(Ref<TextureResource> texture);

		static Ref<UnlitMaterial> Create(const std::string& name = "UnlitMaterial");

	private:
		void InitializeParameters();
	};

	// Custom material
	class CustomMaterial : public Material {
	public:
		CustomMaterial(const std::string& name, Ref<ShaderProgram> shaderProgram);
		virtual ~CustomMaterial() = default;

		static Ref<CustomMaterial> Create(const std::string& name, Ref<ShaderProgram> shaderProgram);
	};

}
