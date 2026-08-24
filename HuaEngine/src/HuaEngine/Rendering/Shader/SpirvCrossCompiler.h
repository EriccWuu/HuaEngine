#pragma once

#include <string>
#include <vector>

#include "HuaEngine/Core/ResultEnvelope.h"
#include "HuaEngine/Rendering/Shader/ShaderInterface.h"

namespace HE::Rendering {
	struct OpenGlCombinedSampler {
		std::string TextureName;
		std::string SamplerName;
		std::string UniformName;
	};

	struct SpirvCrossOutput {
		std::string Glsl;
		std::vector<OpenGlCombinedSampler> CombinedSamplers;
		std::string CompilerIdentity;
	};

	ResultEnvelope CompileSpirvToOpenGlGlsl(
		const std::vector<uint32_t>& spirv,
		ShaderStage stage,
		SpirvCrossOutput& output);

	ResultEnvelope ValidateOpenGlGlsl(
		std::string_view glsl,
		ShaderStage stage);
}
