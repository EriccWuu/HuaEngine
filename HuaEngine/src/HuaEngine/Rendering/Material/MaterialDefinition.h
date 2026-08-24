#pragma once

#include <string>
#include <vector>

#include "HuaEngine/Core/Sha256.h"
#include "HuaEngine/Rendering/Shader/ShaderInterface.h"

namespace HE::Rendering {
	struct MaterialOverrideSet;
	struct MaterialParameterDefinition {
		std::string Name;
		std::string DisplayName;
		ShaderValueType Type = ShaderValueType::Float;
		ShaderEditorKind Editor = ShaderEditorKind::Default;
		ShaderParameterValue DefaultValue = 0.0f;
		ShaderParameterValue CurrentValue = 0.0f;
		std::vector<float> Range;
		float Step = 0.0f;
		std::string Tooltip;
	};

	class MaterialDefinition final {
	public:
		MaterialDefinition() = default;
		MaterialDefinition(std::vector<MaterialParameterDefinition> parameters, Sha256Digest digest)
			: m_Parameters(std::move(parameters)), m_Digest(digest), m_Signature(Sha256Prefix64(digest)) {}

		[[nodiscard]] const std::vector<MaterialParameterDefinition>& GetParameters() const { return m_Parameters; }
		[[nodiscard]] const Sha256Digest& GetDigest() const { return m_Digest; }
		[[nodiscard]] uint64_t GetSignature() const { return m_Signature; }
		[[nodiscard]] const std::string& GetMaterialGuid() const { return m_MaterialGuid; }
		[[nodiscard]] const std::string& GetShaderGuid() const { return m_ShaderGuid; }
		[[nodiscard]] const Sha256Digest& GetShaderInterfaceDigest() const { return m_ShaderInterfaceDigest; }
		[[nodiscard]] uint64_t GetShaderInterfaceSignature() const { return m_ShaderInterfaceSignature; }
		void SetIdentity(std::string materialGuid, std::string shaderGuid, Sha256Digest shaderDigest, uint64_t shaderSignature) {
			m_MaterialGuid = std::move(materialGuid); m_ShaderGuid = std::move(shaderGuid); m_ShaderInterfaceDigest = shaderDigest; m_ShaderInterfaceSignature = shaderSignature;
		}

	private:
		std::vector<MaterialParameterDefinition> m_Parameters;
		Sha256Digest m_Digest{};
		uint64_t m_Signature = 0;
		std::string m_MaterialGuid;
		std::string m_ShaderGuid;
		Sha256Digest m_ShaderInterfaceDigest{};
		uint64_t m_ShaderInterfaceSignature = 0;
	};

	bool ReconcileMaterialOverrides(MaterialOverrideSet& overrides, const MaterialDefinition& definition);
}
