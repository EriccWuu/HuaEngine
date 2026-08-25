#include "enginepch.h"
#include "PngTextureImporter.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <set>
#include <stdexcept>

#include "stb_image.h"

namespace {
	constexpr uint64_t MaxImportedTextureBytes = 1024ull * 1024ull * 1024ull;
}

namespace HE {
	namespace {
		bool ParseBool(std::string_view value, bool& output) { if (value == "true") { output = true; return true; } if (value == "false") { output = false; return true; } return false; }
	}

	bool PngTextureImporter::CanImport(AssetKind kind, std::string_view extension) const {
		return kind == AssetKind::Texture2D && extension == ".png";
	}

	std::unique_ptr<AssetImportSettings> PngTextureImporter::CreateDefaultSettings() const { return std::make_unique<PngTextureImportSettings>(); }

	ResultEnvelope PngTextureImporter::DecodeSettings(const AssetMetaSettingsNode& source, std::unique_ptr<AssetImportSettings>& output) const {
		if (source.Values.empty()) { output = CreateDefaultSettings(); return ResultEnvelope::Success("asset.import.settings.decode", std::string(GetId()), "Default PNG import settings decoded"); }
		static const std::set<std::string> keys = { "alpha_mode", "color_space", "compression", "generate_mipmaps", "max_size" };
		if (source.Values.size() != keys.size()) return ResultEnvelope::Failure("asset.import.settings.decode", std::string(GetId()), "PNG import settings are incomplete");
		for (const auto& [key, value] : source.Values) if (!keys.contains(key)) return ResultEnvelope::Failure("asset.import.settings.decode", std::string(GetId()), "PNG import settings contain an unknown field");
		auto settings = std::make_unique<PngTextureImportSettings>();
		const auto& color = source.Values.at("color_space"); settings->ColorSpace = color == "linear" ? TextureColorSpace::Linear : TextureColorSpace::Srgb;
		const auto& alpha = source.Values.at("alpha_mode"); settings->AlphaMode = alpha == "opaque" ? TextureAlphaMode::Opaque : TextureAlphaMode::Preserve;
		try { size_t consumed = 0; const auto& value = source.Values.at("max_size"); const auto parsed = std::stoul(value, &consumed); if (consumed != value.size() || parsed > std::numeric_limits<uint32_t>::max()) throw std::invalid_argument("max_size"); settings->MaxSize = static_cast<uint32_t>(parsed); } catch (...) { return ResultEnvelope::Failure("asset.import.settings.decode", std::string(GetId()), "PNG maximum size is invalid"); }
		if ((color != "srgb" && color != "linear") || (alpha != "preserve" && alpha != "opaque") || source.Values.at("compression") != "none" || !ParseBool(source.Values.at("generate_mipmaps"), settings->GenerateMipmaps))
			return ResultEnvelope::Failure("asset.import.settings.decode", std::string(GetId()), "PNG import settings contain an invalid value");
		auto result = ValidateSettings(*settings); if (result.Succeeded()) output = std::move(settings); return result;
	}

	ResultEnvelope PngTextureImporter::EncodeSettings(const AssetImportSettings& source, AssetMetaSettingsNode& output) const {
		const auto* settings = dynamic_cast<const PngTextureImportSettings*>(&source);
		if (!settings) return ResultEnvelope::Failure("asset.import.settings.encode", std::string(GetId()), "PNG import settings type is invalid");
		auto result = ValidateSettings(*settings); if (!result.Succeeded()) return result;
		output.Values = { { "alpha_mode", settings->AlphaMode == TextureAlphaMode::Opaque ? "opaque" : "preserve" }, { "color_space", settings->ColorSpace == TextureColorSpace::Linear ? "linear" : "srgb" },
			{ "compression", "none" }, { "generate_mipmaps", settings->GenerateMipmaps ? "true" : "false" }, { "max_size", std::to_string(settings->MaxSize) } };
		return ResultEnvelope::Success("asset.import.settings.encode", std::string(GetId()), "PNG import settings encoded");
	}

	ResultEnvelope PngTextureImporter::ValidateSettings(const AssetImportSettings& source) const {
		const auto* settings = dynamic_cast<const PngTextureImportSettings*>(&source);
		if (!settings || settings->MaxSize == 0 || settings->MaxSize > 16384 || settings->GenerateMipmaps || settings->ColorSpace != TextureColorSpace::Srgb)
			return ResultEnvelope::Failure("asset.import.settings.validate", std::string(GetId()), "PNG import settings request an unsupported value");
		return ResultEnvelope::Success("asset.import.settings.validate", std::string(GetId()), "PNG import settings are valid");
	}

	AssetImportResult PngTextureImporter::Import(const AssetImportContext& context) const {
		AssetImportResult result;
		const auto* settings = dynamic_cast<const PngTextureImportSettings*>(context.Settings);
		if (!settings || !ValidateSettings(*settings).Succeeded()) {
			result.Diagnostics.push_back({ DiagnosticSeverity::Error, "asset.import.png_settings_invalid", "PNG import settings are unavailable or unsupported", context.SourcePath.generic_string() });
			return result;
		}
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

		const float resizeScale = (std::min)(1.0f, static_cast<float>(settings->MaxSize) / static_cast<float>((std::max)(width, height)));
		const int outputWidth = (std::max)(1, static_cast<int>(width * resizeScale));
		const int outputHeight = (std::max)(1, static_cast<int>(height * resizeScale));
		TextureArtifactData textureData;
		textureData.Width = static_cast<uint32_t>(outputWidth);
		textureData.Height = static_cast<uint32_t>(outputHeight);
		textureData.Format = TextureArtifactFormat::RGBA8;
		textureData.MipLevels = 1;
		textureData.Pixels.resize(static_cast<size_t>(outputWidth) * outputHeight * 4);
		for (int destinationRow = 0; destinationRow < outputHeight; ++destinationRow) {
			const int sourceRow = height - 1 - destinationRow * height / outputHeight;
			for (int destinationColumn = 0; destinationColumn < outputWidth; ++destinationColumn) {
				const int sourceColumn = destinationColumn * width / outputWidth;
				const auto sourceOffset = (static_cast<size_t>(sourceRow) * width + sourceColumn) * 4;
				const auto destinationOffset = (static_cast<size_t>(destinationRow) * outputWidth + destinationColumn) * 4;
				std::memcpy(textureData.Pixels.data() + destinationOffset, decoded + sourceOffset, 4);
				if (settings->AlphaMode == TextureAlphaMode::Opaque) textureData.Pixels[destinationOffset + 3] = 255;
			}
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
