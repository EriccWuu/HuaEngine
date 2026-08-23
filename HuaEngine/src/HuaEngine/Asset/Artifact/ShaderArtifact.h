#pragma once

#include <string>

#include "HuaEngine/Asset/Library/AssetLibraryTypes.h"
#include "HuaEngine/Core/ResultEnvelope.h"

namespace HE {
	inline constexpr uint32_t ShaderArtifactVersion = 1;

	struct ShaderArtifactData {
		std::string VertexSource;
		std::string FragmentSource;
	};

	ResultEnvelope EncodeShaderArtifact(
		const ShaderArtifactData& shader,
		AssetArtifact& outArtifact);

	ResultEnvelope DecodeShaderArtifact(
		const AssetArtifact& artifact,
		ShaderArtifactData& outShader);
}
