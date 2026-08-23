#include "enginepch.h"
#include "GlslShaderImporter.h"

#include <fstream>

namespace {
	constexpr uint64_t MaxShaderSourceBytes = 16ull * 1024ull * 1024ull;

	std::string_view Trim(std::string_view value) {
		const auto begin = value.find_first_not_of(" \t\r");
		if (begin == std::string_view::npos) return {};
		const auto end = value.find_last_not_of(" \t\r");
		return value.substr(begin, end - begin + 1);
	}

	bool ParseCombinedShader(
		std::string_view source,
		HE::ShaderArtifactData& outShader,
		std::string& outError) {
		outShader = {};
		constexpr std::string_view marker = "#type";
		size_t markerOffset = source.find(marker);
		while (markerOffset != std::string_view::npos) {
			const size_t lineEnd = source.find_first_of("\r\n", markerOffset);
			if (lineEnd == std::string_view::npos) {
				outError = "Shader stage declaration is missing source";
				return false;
			}

			const auto stageName = Trim(source.substr(markerOffset + marker.size(), lineEnd - markerOffset - marker.size()));
			const size_t sourceBegin = source.find_first_not_of("\r\n", lineEnd);
			if (sourceBegin == std::string_view::npos) {
				outError = "Shader stage source is empty";
				return false;
			}
			const size_t nextMarker = source.find(marker, sourceBegin);
			const auto stageSource = source.substr(
				sourceBegin,
				nextMarker == std::string_view::npos ? source.size() - sourceBegin : nextMarker - sourceBegin);
			if (Trim(stageSource).empty()) {
				outError = "Shader stage source is empty";
				return false;
			}

			std::string* destination = nullptr;
			if (stageName == "vertex") destination = &outShader.VertexSource;
			else if (stageName == "fragment" || stageName == "pixel") destination = &outShader.FragmentSource;
			else {
				outError = "Unsupported shader stage: " + std::string(stageName);
				return false;
			}
			if (!destination->empty()) {
				outError = "Shader contains a duplicate stage: " + std::string(stageName);
				return false;
			}
			destination->assign(stageSource);
			markerOffset = nextMarker;
		}

		if (outShader.VertexSource.empty() || outShader.FragmentSource.empty()) {
			outError = "Shader requires exactly one vertex and one fragment stage";
			return false;
		}
		return true;
	}
}

namespace HE {
	bool GlslShaderImporter::CanImport(AssetKind kind, std::string_view extension) const {
		return kind == AssetKind::Shader && extension == ".glsl";
	}

	AssetImportResult GlslShaderImporter::Import(const AssetImportContext& context) const {
		AssetImportResult result;
		std::error_code errorCode;
		const auto fileSize = std::filesystem::file_size(context.SourcePath, errorCode);
		if (errorCode || fileSize == 0 || fileSize > MaxShaderSourceBytes) {
			result.Diagnostics.push_back({
				DiagnosticSeverity::Error,
				"asset.import.shader_file_invalid",
				"GLSL shader source is empty, unavailable, or too large",
				context.SourcePath.generic_string()
			});
			return result;
		}

		std::ifstream stream(context.SourcePath, std::ios::in | std::ios::binary);
		std::string source(static_cast<size_t>(fileSize), '\0');
		stream.read(source.data(), static_cast<std::streamsize>(source.size()));
		if (!stream || stream.gcount() != static_cast<std::streamsize>(source.size())) {
			result.Diagnostics.push_back({
				DiagnosticSeverity::Error,
				"asset.import.shader_read_failed",
				"Failed to read GLSL shader source",
				context.SourcePath.generic_string()
			});
			return result;
		}

		ShaderArtifactData shader;
		std::string parseError;
		if (!ParseCombinedShader(source, shader, parseError)) {
			result.Diagnostics.push_back({
				DiagnosticSeverity::Error,
				"asset.import.shader_parse_failed",
				std::move(parseError),
				context.SourcePath.generic_string()
			});
			return result;
		}

		auto encodeResult = EncodeShaderArtifact(shader, result.Artifact);
		if (!encodeResult.Succeeded()) {
			result.Diagnostics = std::move(encodeResult.Details);
			return result;
		}
		result.Success = true;
		return result;
	}
}
