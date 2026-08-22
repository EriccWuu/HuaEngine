#pragma once

#include <cstdint>
#include <vector>

#include "HuaEngine/Asset/Library/AssetLibraryTypes.h"
#include "HuaEngine/Core/ResultEnvelope.h"

namespace HE {
	inline constexpr uint32_t TextureArtifactVersion = 1;

	enum class TextureArtifactFormat : uint32_t {
		RGBA8 = 1
	};

	struct TextureArtifactData {
		uint32_t Width = 0;
		uint32_t Height = 0;
		TextureArtifactFormat Format = TextureArtifactFormat::RGBA8;
		uint32_t MipLevels = 1;
		std::vector<uint8_t> Pixels;
	};

	ResultEnvelope EncodeTextureArtifact(
		const TextureArtifactData& texture,
		AssetArtifact& outArtifact);

	ResultEnvelope DecodeTextureArtifact(
		const AssetArtifact& artifact,
		TextureArtifactData& outTexture);
}
