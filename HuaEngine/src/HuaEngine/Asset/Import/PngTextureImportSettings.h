#pragma once

#include "HuaEngine/Asset/Metadata/AssetImportSettings.h"

namespace HE {
	enum class TextureColorSpace { Srgb, Linear };
	enum class TextureAlphaMode { Preserve, Opaque };

	struct PngTextureImportSettings final : AssetImportSettings {
		TextureColorSpace ColorSpace = TextureColorSpace::Srgb;
		bool GenerateMipmaps = false;
		TextureAlphaMode AlphaMode = TextureAlphaMode::Preserve;
		uint32_t MaxSize = 4096;

		[[nodiscard]] std::string_view GetImporterId() const override { return "hua.texture-png"; }
		[[nodiscard]] bool operator==(const PngTextureImportSettings& other) const {
			return ColorSpace == other.ColorSpace && GenerateMipmaps == other.GenerateMipmaps && AlphaMode == other.AlphaMode && MaxSize == other.MaxSize;
		}
	};
}
