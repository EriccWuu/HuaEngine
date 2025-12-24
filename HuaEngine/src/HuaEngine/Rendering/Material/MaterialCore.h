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

	// 材质参数的变体类型
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
		Standard,   // PBR 材质
		Unlit,      // 无光照材质
		Custom      // 自定义材质
	};

	class MaterialInstance; // 前向声明

	class Material : public std::enable_shared_from_this<Material> {
	public:
		Material() = default;
		Material(const std::string& name, MaterialType type = MaterialType::Empty);
		virtual ~Material() = default;

		// 静态创建函数，用于反序列化
		static Ref<Material> Create(const std::string& name, MaterialType type = MaterialType::Empty);
		static Ref<Material> CreateFromDeserialization(); // 用于反序列化时的临时创建

		// 基本属性
		const std::string& GetName() const { return m_Name; }
		MaterialType GetType() const { return m_Type; }
		Ref<Shader> GetShader() const { return m_Shader; }
		void SetShader(Ref<Shader> shader) { m_Shader = shader; }
		
		// 反序列化用的 set 接口
		void SetName(const std::string& name) { m_Name = name; }
		void SetType(MaterialType type) { m_Type = type; }

		// 参数管理
		void AddParameter(const MaterialParameter& parameter);
		bool HasParameter(const std::string& name) const;
		const MaterialParameter* GetParameter(const std::string& name) const;
		const std::unordered_map<std::string, MaterialParameter>& GetParameters() const { return m_Parameters; }
		
		// 反序列化用的参数管理接口
		void SetParameters(const std::unordered_map<std::string, MaterialParameter>& parameters) { m_Parameters = parameters; }
		void ClearParameters() { m_Parameters.clear(); }

		// 纹理槽管理
		void SetTextureSlot(const std::string& name, uint32_t slot);
		uint32_t GetTextureSlot(const std::string& name) const;
		const std::unordered_map<std::string, uint32_t>& GetTextureSlots() const { return m_TextureSlots; }
		
		// 反序列化用的纹理槽接口
		void SetTextureSlots(const std::unordered_map<std::string, uint32_t>& slots) { m_TextureSlots = slots; }
		void ClearTextureSlots() { m_TextureSlots.clear(); m_NextTextureSlot = 0; }

		// 材质操作
		virtual void Bind();
		virtual void Unbind();
		virtual void SetParameter(const std::string& name, const MaterialParameterValue& value);

		// 材质实例创建
		virtual Ref<MaterialInstance> CreateInstance();

		// 序列化支持（后续实现）
		virtual void Serialize() {}
		virtual void Deserialize() {}

		// 应用参数到 Shader (MaterialInstance 需要访问)
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

		// 基本属性
		Ref<Material> GetBaseMaterial() const { return m_BaseMaterial; }
		Ref<Shader> GetShader() const { return m_BaseMaterial->GetShader(); }

		// 参数覆盖
		void SetParameter(const std::string& name, const MaterialParameterValue& value);
		bool HasParameterOverride(const std::string& name) const;
		const MaterialParameter* GetParameterOverride(const std::string& name) const;
		const std::unordered_map<std::string, MaterialParameter>& GetParameterOverrides() const { return m_ParameterOverrides; }
		
		// 反序列化用的接口
		void ClearParameterOverrides() { m_ParameterOverrides.clear(); }
		void SetParameterOverrides(const std::unordered_map<std::string, MaterialParameter>& overrides) { m_ParameterOverrides = overrides; }

		// 材质操作
		void Bind();
		void Unbind();
		void ApplyParameters();

	private:
		Ref<Material> m_BaseMaterial;
		std::unordered_map<std::string, MaterialParameter> m_ParameterOverrides;
	};

}