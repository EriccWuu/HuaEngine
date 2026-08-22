#include "enginepch.h"
#include "TextureArtifact.h"

#include <limits>

#include "HuaEngine/Asset/Library/AssetBinaryIO.h"

namespace {
	constexpr uint64_t BytesPerRgba8Pixel = 4;
	constexpr uint64_t MaxTextureBytes = 1024ull * 1024ull * 1024ull;

	bool TryGetExpectedPixelBytes(const HE::TextureArtifactData& texture, uint64_t& outBytes) {
		if (texture.Width == 0 || texture.Height == 0 || texture.MipLevels != 1 ||
			texture.Format != HE::TextureArtifactFormat::RGBA8) {
			return false;
		}
		outBytes = static_cast<uint64_t>(texture.Width) * texture.Height * BytesPerRgba8Pixel;
		return outBytes <= MaxTextureBytes;
	}

	HE::ResultEnvelope MakeTextureArtifactFailure(std::string operation, std::string code, std::string message) {
		auto result = HE::ResultEnvelope::Failure(std::move(operation), "asset:texture", message);
		result.AddDetail({ HE::DiagnosticSeverity::Error, std::move(code), std::move(message), {} });
		return result;
	}
}

namespace HE {
	ResultEnvelope EncodeTextureArtifact(
		const TextureArtifactData& texture,
		AssetArtifact& outArtifact) {
		outArtifact = {};
		uint64_t expectedBytes = 0;
		if (!TryGetExpectedPixelBytes(texture, expectedBytes) || texture.Pixels.size() != expectedBytes) {
			return MakeTextureArtifactFailure(
				"asset.texture_artifact.encode",
				"asset.texture_artifact.invalid",
				"Texture artifact data is invalid");
		}

		AssetBinaryWriter writer;
		writer.WriteU32(texture.Width);
		writer.WriteU32(texture.Height);
		writer.WriteU32(static_cast<uint32_t>(texture.Format));
		writer.WriteU32(texture.MipLevels);
		writer.WriteU64(expectedBytes);
		writer.WriteBytes(texture.Pixels);

		outArtifact.Kind = AssetKind::Texture2D;
		outArtifact.ArtifactVersion = TextureArtifactVersion;
		outArtifact.Payload = writer.TakeData();
		return ResultEnvelope::Success("asset.texture_artifact.encode", "asset:texture", "Texture artifact encoded");
	}

	ResultEnvelope DecodeTextureArtifact(
		const AssetArtifact& artifact,
		TextureArtifactData& outTexture) {
		outTexture = {};
		if (artifact.Kind != AssetKind::Texture2D || artifact.ArtifactVersion != TextureArtifactVersion) {
			return MakeTextureArtifactFailure(
				"asset.texture_artifact.decode",
				"asset.texture_artifact.version_mismatch",
				"Texture artifact kind or version is unsupported");
		}

		AssetBinaryReader reader(artifact.Payload);
		uint32_t format = 0;
		uint64_t storedBytes = 0;
		if (!reader.ReadU32(outTexture.Width) || !reader.ReadU32(outTexture.Height) ||
			!reader.ReadU32(format) || !reader.ReadU32(outTexture.MipLevels) || !reader.ReadU64(storedBytes)) {
			return MakeTextureArtifactFailure(
				"asset.texture_artifact.decode",
				"asset.texture_artifact.header_invalid",
				"Texture artifact header is invalid");
		}
		outTexture.Format = static_cast<TextureArtifactFormat>(format);

		uint64_t expectedBytes = 0;
		if (!TryGetExpectedPixelBytes(outTexture, expectedBytes) || storedBytes != expectedBytes ||
			storedBytes > std::numeric_limits<size_t>::max() ||
			!reader.ReadBytes(static_cast<size_t>(storedBytes), outTexture.Pixels) ||
			reader.Failed() || reader.Remaining() != 0) {
			return MakeTextureArtifactFailure(
				"asset.texture_artifact.decode",
				"asset.texture_artifact.payload_invalid",
				"Texture artifact pixel payload is invalid");
		}

		return ResultEnvelope::Success("asset.texture_artifact.decode", "asset:texture", "Texture artifact decoded");
	}
}
