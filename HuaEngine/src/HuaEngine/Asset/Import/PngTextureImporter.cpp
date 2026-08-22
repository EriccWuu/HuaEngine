#include "enginepch.h"
#include "PngTextureImporter.h"

#include <algorithm>
#include <cstring>
#include <limits>

#include "stb_image.h"

namespace {
	constexpr uint64_t MaxImportedTextureBytes = 1024ull * 1024ull * 1024ull;
}

namespace HE {
	bool PngTextureImporter::CanImport(AssetKind kind, std::string_view extension) const {
		return kind == AssetKind::Texture2D && extension == ".png";
	}

	AssetImportResult PngTextureImporter::Import(const AssetImportContext& context) const {
		AssetImportResult result;
		int width = 0;
		int height = 0;
		int sourceChannels = 0;
		stbi_uc* decoded = stbi_load(context.SourcePath.string().c_str(), &width, &height, &sourceChannels, 4);
		if (!decoded || width <= 0 || height <= 0) {
			if (decoded) stbi_image_free(decoded);
			result.Diagnostics.push_back({
				DiagnosticSeverity::Error,
				"asset.import.png_decode_failed",
				"PNG source file could not be decoded",
				context.SourcePath.generic_string()
			});
			return result;
		}
		(void)sourceChannels;

		const uint64_t rowBytes = static_cast<uint64_t>(width) * 4;
		const uint64_t pixelBytes = rowBytes * static_cast<uint64_t>(height);
		if (pixelBytes > MaxImportedTextureBytes || pixelBytes > std::numeric_limits<size_t>::max()) {
			stbi_image_free(decoded);
			result.Diagnostics.push_back({
				DiagnosticSeverity::Error,
				"asset.import.png_too_large",
				"PNG dimensions exceed the supported artifact size",
				context.SourcePath.generic_string()
			});
			return result;
		}

		TextureArtifactData textureData;
		textureData.Width = static_cast<uint32_t>(width);
		textureData.Height = static_cast<uint32_t>(height);
		textureData.Format = TextureArtifactFormat::RGBA8;
		textureData.MipLevels = 1;
		textureData.Pixels.resize(static_cast<size_t>(pixelBytes));
		for (int destinationRow = 0; destinationRow < height; ++destinationRow) {
			const int sourceRow = height - destinationRow - 1;
			std::memcpy(
				textureData.Pixels.data() + static_cast<size_t>(destinationRow) * static_cast<size_t>(rowBytes),
				decoded + static_cast<size_t>(sourceRow) * static_cast<size_t>(rowBytes),
				static_cast<size_t>(rowBytes));
		}
		stbi_image_free(decoded);

		auto encodeResult = EncodeTextureArtifact(textureData, result.Artifact);
		if (!encodeResult.Succeeded()) {
			result.Diagnostics = std::move(encodeResult.Details);
			return result;
		}

		result.Success = true;
		return result;
	}
}
