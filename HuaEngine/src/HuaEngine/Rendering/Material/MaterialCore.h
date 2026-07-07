#pragma once

#include "HuaEngine/Core/Core.h"
#include "../Shader/Shader.h"
#include "../Texture.h"
#include <unordered_map>
#include <variant>
#include <string>
#include "glm/glm.hpp"

namespace HE::Rendering {

	enum class MaterialParameterType {
		Int, Float, Vec2, Vec3, Vec4, Mat3, Mat4,
		Texture2D, TextureCube, IntArray, FloatArray
	};

	// Material parameter variant type
	using MaterialParameterValue = std::variant<
		int, float, glm::vec2, glm::vec3, glm::vec4, glm::mat3, glm::mat4,
		Ref<Texture2D>, std::vector<int>, std::vector<float>
	>;

	struct MaterialParameter {
		std::string Name;
		MaterialParameterType Type;
		MaterialParameterValue Value;

		MaterialParameter() = default;
		MaterialParameter(const std::string& name, MaterialParameterType type, const MaterialParameterValue& defaultValue)
			: Name(name), Type(type), Value(defaultValue) {}
	};

	enum class MaterialType {
		Empty,
		Standard,   // PBR material
		Unlit,      // Unlit material
		Custom      // Custom material
	};

	class MaterialInstance; // Forward declaration

	class Material : public std::enable_shared_from_this<Material> {
	public:
		Material() = default;
		Material(const std::string& name, MaterialType type = MaterialType::Empty);
		virtual ~Material() = default;

		// Static creation function for deserialization
		static Ref<Material> Create(const std::string& name, MaterialType type = MaterialType::Empty);
		static Ref<Material> CreateFromDeserialization(); // For temporary creation during deserialization

		// Basic properties
		const std::string& GetName() const { return m_Name; }
		MaterialType GetType() const { return m_Type; }
		Ref<Shader> GetShader() const { return m_Shader; }
		void SetShader(Ref<Shader> shader) { m_Shader = shader; }

		// Setter interface for deserialization
		void SetName(const std::string& name) { m_Name = name; }
		void SetType(MaterialType type) { m_Type = type; }

		// Parameter management
		void AddParameter(const MaterialParameter& parameter);
		bool HasParameter(const std::string& name) const;
		const MaterialParameter* GetParameter(const std::string& name) const;
		const std::unordered_map<std::string, MaterialParameter>& GetParameters() const { return m_Parameters; }

		// Parameter management interface for deserialization
		void SetParameters(const std::unordered_map<std::string, MaterialParameter>& parameters) { m_Parameters = parameters; }
		void ClearParameters() { m_Parameters.clear(); }

		// Texture slot management
		void SetTextureSlot(const std::string& name, uint32_t slot);
		uint32_t GetTextureSlot(const std::string& name) const;
		const std::unordered_map<std::string, uint32_t>& GetTextureSlots() const { return m_TextureSlots; }

		// Texture slot interface for deserialization
		void SetTextureSlots(const std::unordered_map<std::string, uint32_t>& slots) { m_TextureSlots = slots; }
		void ClearTextureSlots() { m_TextureSlots.clear(); m_NextTextureSlot = 0; }

		// Legacy compatibility path. Normal render passes should use RenderResourceResolver
		// to build MaterialBinding and submit it through CommandList::SetMaterialBinding.
		virtual void Bind();
		virtual void Unbind();
		virtual void SetParameter(const std::string& name, const MaterialParameterValue& value);

		// Create material instance
		virtual Ref<MaterialInstance> CreateInstance();

		// Serialization support (to be implemented)
		virtual void Serialize() {}
		virtual void Deserialize() {}

		// Apply parameter to shader for legacy MaterialInstance paths.
		// Normal render passes should submit MaterialBinding through CommandList.
		void ApplyParameter(const std::string& name, const MaterialParameterValue& value);

	protected:
		std::string m_Name;
		MaterialType m_Type;
		Ref<Shader> m_Shader;
		std::unordered_map<std::string, MaterialParameter> m_Parameters;
		std::unordered_map<std::string, uint32_t> m_TextureSlots;
		uint32_t m_NextTextureSlot = 0;
	};

	class MaterialInstance {
	public:
		MaterialInstance() = default;
		MaterialInstance(Ref<Material> baseMaterial);
		~MaterialInstance() = default;

		// Basic properties
		Ref<Material> GetBaseMaterial() const { return m_BaseMaterial; }
		void SetBaseMaterial(Ref<Material> baseMaterial) { m_BaseMaterial = std::move(baseMaterial); }
		Ref<Shader> GetShader() const { return m_BaseMaterial ? m_BaseMaterial->GetShader() : nullptr; }

		// Parameter overrides
		void SetParameter(const std::string& name, const MaterialParameterValue& value);
		bool HasParameterOverride(const std::string& name) const;
		const MaterialParameter* GetParameterOverride(const std::string& name) const;
		const std::unordered_map<std::string, MaterialParameter>& GetParameterOverrides() const { return m_ParameterOverrides; }

		// Interface for deserialization
		void ClearParameterOverrides() { m_ParameterOverrides.clear(); }
		void SetParameterOverrides(const std::unordered_map<std::string, MaterialParameter>& overrides) { m_ParameterOverrides = overrides; }

		// Legacy compatibility path. Normal render passes should use MaterialBinding.
		void Bind();
		void Unbind();
		void ApplyParameters();

	private:
		Ref<Material> m_BaseMaterial;
		std::unordered_map<std::string, MaterialParameter> m_ParameterOverrides;
	};

}
