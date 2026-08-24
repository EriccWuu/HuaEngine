#pragma once

#include <string>
#include <vector>

#include "HuaEngine/Asset/Import/AssetImportFingerprint.h"
#include "HuaEngine/Asset/Library/AssetLibraryTypes.h"
#include "HuaEngine/Core/ResultEnvelope.h"
#include "HuaEngine/Rendering/Shader/ShaderInterface.h"
#include "HuaEngine/Rendering/Shader/SpirvCrossCompiler.h"

namespace HE {
	inline constexpr uint32_t ShaderArtifactVersion = 2;

	struct ShaderStageArtifact {
		Rendering::ShaderStage Stage = Rendering::ShaderStage::Vertex;
		std::string EntryPoint;
		std::string Profile;
		std::vector<uint32_t> Spirv;
		std::string GeneratedOpenGlGlsl;
	};

	struct ShaderArtifactDataV2 {
		std::string SourceLanguage = "HLSL";
		std::string CompilerIdentity;
		std::vector<std::string> CompileOptions;
		std::vector<ShaderStageArtifact> Stages;
		Rendering::ShaderInterface Interface;
		std::vector<Rendering::OpenGlCombinedSampler> OpenGlCombinedSamplers;
		std::vector<AssetImportSourceInput> ImportInputs;
	};

	ResultEnvelope ValidateShaderArtifactV2Contract(const ShaderArtifactDataV2& shader);
	ResultEnvelope EncodeShaderArtifactV2(const ShaderArtifactDataV2& shader, AssetArtifact& outArtifact);
	ResultEnvelope DecodeShaderArtifactV2(const AssetArtifact& artifact, ShaderArtifactDataV2& outShader);
}
