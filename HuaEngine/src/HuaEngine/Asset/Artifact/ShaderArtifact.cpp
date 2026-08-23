#include "enginepch.h"
#include "ShaderArtifact.h"

#include "HuaEngine/Asset/Library/AssetBinaryIO.h"

namespace {
	HE::ResultEnvelope MakeShaderArtifactFailure(
		std::string operation,
		std::string code,
		std::string message) {
		auto result = HE::ResultEnvelope::Failure(std::move(operation), "asset:shader", message);
		result.AddDetail({ HE::DiagnosticSeverity::Error, std::move(code), std::move(message), {} });
		return result;
	}
}

namespace HE {
	ResultEnvelope EncodeShaderArtifact(
		const ShaderArtifactData& shader,
		AssetArtifact& outArtifact) {
		outArtifact = {};
		if (shader.VertexSource.empty() || shader.FragmentSource.empty()) {
			return MakeShaderArtifactFailure(
				"asset.shader_artifact.encode",
				"asset.shader_artifact.invalid",
				"Shader artifact requires vertex and fragment source");
		}

		AssetBinaryWriter writer;
		writer.WriteString(shader.VertexSource);
		writer.WriteString(shader.FragmentSource);
		outArtifact.Kind = AssetKind::Shader;
		outArtifact.ArtifactVersion = ShaderArtifactVersion;
		outArtifact.Payload = writer.TakeData();
		return ResultEnvelope::Success("asset.shader_artifact.encode", "asset:shader", "Shader artifact encoded");
	}

	ResultEnvelope DecodeShaderArtifact(
		const AssetArtifact& artifact,
		ShaderArtifactData& outShader) {
		outShader = {};
		if (artifact.Kind != AssetKind::Shader || artifact.ArtifactVersion != ShaderArtifactVersion) {
			return MakeShaderArtifactFailure(
				"asset.shader_artifact.decode",
				"asset.shader_artifact.version_mismatch",
				"Shader artifact kind or version is unsupported");
		}

		AssetBinaryReader reader(artifact.Payload);
		if (!reader.ReadString(outShader.VertexSource) ||
			!reader.ReadString(outShader.FragmentSource) ||
			outShader.VertexSource.empty() ||
			outShader.FragmentSource.empty() ||
			reader.Failed() ||
			reader.Remaining() != 0) {
			outShader = {};
			return MakeShaderArtifactFailure(
				"asset.shader_artifact.decode",
				"asset.shader_artifact.payload_invalid",
				"Shader artifact payload is malformed");
		}

		return ResultEnvelope::Success("asset.shader_artifact.decode", "asset:shader", "Shader artifact decoded");
	}
}
