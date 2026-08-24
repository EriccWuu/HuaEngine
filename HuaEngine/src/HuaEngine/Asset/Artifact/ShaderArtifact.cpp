#include "enginepch.h"
#include "ShaderArtifact.h"

#include <cctype>
#include <set>

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

	void WriteParameterValue(HE::AssetBinaryWriter& writer, const HE::Rendering::ShaderParameterValue& value) {
		std::visit([&](const auto& item) {
			using T = std::decay_t<decltype(item)>;
			if constexpr (std::is_same_v<T, int32_t>) writer.WriteU32(static_cast<uint32_t>(item));
			else if constexpr (std::is_same_v<T, float>) writer.WriteFloat(item);
			else if constexpr (std::is_same_v<T, std::string>) writer.WriteString(item);
			else { const auto* values = reinterpret_cast<const float*>(&item); for (size_t index = 0; index < sizeof(T) / sizeof(float); ++index) writer.WriteFloat(values[index]); }
		}, value);
	}

	bool ReadParameterValue(HE::AssetBinaryReader& reader, HE::Rendering::ShaderValueType type, HE::Rendering::ShaderParameterValue& value) {
		using namespace HE::Rendering;
		if (type == ShaderValueType::Int) { uint32_t item = 0; if (!reader.ReadU32(item)) return false; value = static_cast<int32_t>(item); return true; }
		if (type == ShaderValueType::Texture2D || type == ShaderValueType::SamplerState) { std::string item; if (!reader.ReadString(item)) return false; value = std::move(item); return true; }
		const size_t count = type == ShaderValueType::Float ? 1 : type == ShaderValueType::Float2 ? 2 : type == ShaderValueType::Float3 ? 3 : type == ShaderValueType::Float4 ? 4 : type == ShaderValueType::Float4x4 ? 16 : 0;
		if (count == 0) return false;
		std::array<float, 16> items{};
		for (size_t index = 0; index < count; ++index) if (!reader.ReadFloat(items[index])) return false;
		if (count == 1) value = items[0]; else if (count == 2) value = glm::vec2(items[0], items[1]); else if (count == 3) value = glm::vec3(items[0], items[1], items[2]); else if (count == 4) value = glm::vec4(items[0], items[1], items[2], items[3]);
		else { glm::mat4 matrix; std::memcpy(&matrix, items.data(), sizeof(matrix)); value = matrix; }
		return true;
	}

	bool IsHexDigest(std::string_view value) {
		return value.size() == 64 && std::all_of(value.begin(), value.end(), [](unsigned char character) {
			return std::isxdigit(character) != 0;
		});
	}

	bool IsSafeInputPath(std::string_view value) {
		if (value.empty()) return false;
		const std::filesystem::path path(value);
		return !path.is_absolute() && !path.has_root_name() &&
			std::none_of(path.begin(), path.end(), [](const auto& part) { return part == ".."; });
	}
}

namespace HE {
	ResultEnvelope ValidateShaderArtifactV2Contract(const ShaderArtifactDataV2& shader) {
		if (shader.SourceLanguage != "HLSL" || shader.CompilerIdentity.empty() || shader.Stages.size() != 2 || shader.ImportInputs.empty()) {
			return MakeShaderArtifactFailure("asset.shader_artifact.validate", "asset.shader_artifact.metadata_invalid", "Shader artifact V2 metadata is incomplete");
		}
		std::set<Rendering::ShaderStage> stages;
		for (const auto& stage : shader.Stages) {
			if ((stage.Stage != Rendering::ShaderStage::Vertex && stage.Stage != Rendering::ShaderStage::Fragment) ||
				!stages.emplace(stage.Stage).second || stage.EntryPoint.empty() || stage.Profile.empty() ||
				stage.Spirv.size() < 5 || stage.Spirv.front() != 0x07230203u || stage.GeneratedOpenGlGlsl.empty()) {
				return MakeShaderArtifactFailure("asset.shader_artifact.validate", "asset.shader_artifact.stage_invalid", "Shader artifact stage contract is invalid");
			}
			const auto interfaceStage = std::find_if(shader.Interface.Gpu.Stages.begin(), shader.Interface.Gpu.Stages.end(), [&](const auto& value) {
				return value.Stage == stage.Stage && value.EntryPoint == stage.EntryPoint;
			});
			if (interfaceStage == shader.Interface.Gpu.Stages.end()) {
				return MakeShaderArtifactFailure("asset.shader_artifact.validate", "asset.shader_artifact.stage_mismatch", "Shader artifact stage does not match ShaderInterface");
			}
		}

		auto finalizedInterface = shader.Interface;
		const auto gpuDigest = finalizedInterface.Gpu.Digest;
		const auto gpuSignature = finalizedInterface.Gpu.Signature;
		const auto authoringDigest = finalizedInterface.Authoring.Digest;
		const auto authoringSignature = finalizedInterface.Authoring.Signature;
		auto interfaceResult = Rendering::FinalizeShaderInterface(finalizedInterface);
		if (!interfaceResult.Succeeded() || finalizedInterface.Gpu.Digest != gpuDigest ||
			finalizedInterface.Gpu.Signature != gpuSignature || finalizedInterface.Authoring.Digest != authoringDigest ||
			finalizedInterface.Authoring.Signature != authoringSignature) {
			return MakeShaderArtifactFailure("asset.shader_artifact.validate", "asset.shader_artifact.interface_invalid", "Shader artifact interface identity is invalid");
		}

		std::set<std::string> inputPaths;
		for (const auto& input : shader.ImportInputs) {
			if (!IsSafeInputPath(input.NormalizedPath) || !IsHexDigest(input.ContentHash) || !inputPaths.emplace(input.NormalizedPath).second) {
				return MakeShaderArtifactFailure("asset.shader_artifact.validate", "asset.shader_artifact.input_invalid", "Shader artifact import input is invalid");
			}
		}

		std::set<std::string> combinedUniforms;
		for (const auto& sampler : shader.OpenGlCombinedSamplers) {
			const auto texture = std::find_if(shader.Interface.Gpu.Resources.begin(), shader.Interface.Gpu.Resources.end(), [&](const auto& value) {
				return value.Name == sampler.TextureName && value.Type == Rendering::ShaderResourceType::Texture2D;
			});
			const auto samplerResource = std::find_if(shader.Interface.Gpu.Resources.begin(), shader.Interface.Gpu.Resources.end(), [&](const auto& value) {
				return value.Name == sampler.SamplerName && value.Type == Rendering::ShaderResourceType::Sampler;
			});
			if (sampler.UniformName.empty() || texture == shader.Interface.Gpu.Resources.end() || samplerResource == shader.Interface.Gpu.Resources.end() ||
				!combinedUniforms.emplace(sampler.UniformName).second) {
				return MakeShaderArtifactFailure("asset.shader_artifact.validate", "asset.shader_artifact.sampler_invalid", "Shader artifact combined sampler map is invalid");
			}
		}
		return ResultEnvelope::Success("asset.shader_artifact.validate", "asset:shader", "Shader artifact V2 contract validated");
	}

	ResultEnvelope EncodeShaderArtifactV2(const ShaderArtifactDataV2& shader, AssetArtifact& outArtifact) {
		outArtifact = {};
		auto validation = ValidateShaderArtifactV2Contract(shader);
		if (!validation.Succeeded()) return validation;
		AssetBinaryWriter writer;
		writer.WriteU32(1);
		writer.WriteString(shader.SourceLanguage);
		writer.WriteString(shader.CompilerIdentity);
		writer.WriteU32(static_cast<uint32_t>(shader.CompileOptions.size()));
		for (const auto& option : shader.CompileOptions) writer.WriteString(option);
		writer.WriteU32(static_cast<uint32_t>(shader.Stages.size()));
		for (const auto& stage : shader.Stages) {
			writer.WriteU8(static_cast<uint8_t>(stage.Stage));
			writer.WriteString(stage.EntryPoint);
			writer.WriteString(stage.Profile);
			writer.WriteU32(static_cast<uint32_t>(stage.Spirv.size()));
			for (const auto word : stage.Spirv) writer.WriteU32(word);
			writer.WriteString(stage.GeneratedOpenGlGlsl);
		}
		const auto& gpu = shader.Interface.Gpu;
		writer.WriteBytes(gpu.Digest);
		writer.WriteU64(gpu.Signature);
		writer.WriteU32(static_cast<uint32_t>(gpu.Stages.size()));
		for (const auto& stage : gpu.Stages) {
			writer.WriteU8(static_cast<uint8_t>(stage.Stage)); writer.WriteString(stage.EntryPoint);
			writer.WriteU32(static_cast<uint32_t>(stage.Inputs.size()));
			for (const auto& input : stage.Inputs) { writer.WriteU32(input.Location); writer.WriteU8(static_cast<uint8_t>(input.Type)); }
			writer.WriteU32(static_cast<uint32_t>(stage.Outputs.size()));
			for (const auto& output : stage.Outputs) { writer.WriteU32(output.Location); writer.WriteU8(static_cast<uint8_t>(output.Type)); }
		}
		writer.WriteU32(static_cast<uint32_t>(gpu.VertexInputs.size()));
		for (const auto& input : gpu.VertexInputs) { writer.WriteString(input.Semantic); writer.WriteU32(input.Location); writer.WriteU8(static_cast<uint8_t>(input.Type)); }
		writer.WriteU32(static_cast<uint32_t>(gpu.Resources.size()));
		for (const auto& resource : gpu.Resources) {
			writer.WriteString(resource.Name); writer.WriteU8(static_cast<uint8_t>(resource.Type)); writer.WriteU32(resource.Set); writer.WriteU32(resource.Binding);
			writer.WriteU32(resource.ArrayCount); writer.WriteU8(resource.StageMask);
		}
		writer.WriteU32(static_cast<uint32_t>(gpu.ConstantBuffers.size()));
		for (const auto& buffer : gpu.ConstantBuffers) {
			writer.WriteString(buffer.Name); writer.WriteU32(buffer.Set); writer.WriteU32(buffer.Binding); writer.WriteU32(buffer.Size); writer.WriteU32(static_cast<uint32_t>(buffer.Members.size()));
			for (const auto& member : buffer.Members) {
				writer.WriteString(member.Name); writer.WriteU8(static_cast<uint8_t>(member.Type)); writer.WriteU32(member.Offset); writer.WriteU32(member.Size);
				writer.WriteU32(member.MatrixStride); writer.WriteU32(member.ArrayStride); writer.WriteU8(member.ColumnMajor ? 1 : 0);
			}
		}
		const auto& authoring = shader.Interface.Authoring;
		writer.WriteBytes(authoring.Digest); writer.WriteU64(authoring.Signature);
		writer.WriteU32(static_cast<uint32_t>(authoring.Parameters.size()));
		for (const auto& parameter : authoring.Parameters) {
			writer.WriteString(parameter.Name); writer.WriteString(parameter.DisplayName); writer.WriteU8(static_cast<uint8_t>(parameter.Scope)); writer.WriteU8(static_cast<uint8_t>(parameter.Type)); writer.WriteU8(static_cast<uint8_t>(parameter.Editor));
			WriteParameterValue(writer, parameter.DefaultValue); writer.WriteU32(static_cast<uint32_t>(parameter.Range.size()));
			for (const auto value : parameter.Range) writer.WriteFloat(value);
			writer.WriteFloat(parameter.Step); writer.WriteString(parameter.Tooltip);
		}
		writer.WriteU32(static_cast<uint32_t>(shader.OpenGlCombinedSamplers.size()));
		for (const auto& sampler : shader.OpenGlCombinedSamplers) { writer.WriteString(sampler.TextureName); writer.WriteString(sampler.SamplerName); writer.WriteString(sampler.UniformName); }
		writer.WriteU32(static_cast<uint32_t>(shader.ImportInputs.size()));
		for (const auto& input : shader.ImportInputs) { writer.WriteString(input.NormalizedPath); writer.WriteString(input.ContentHash); }
		outArtifact.Kind = AssetKind::Shader;
		outArtifact.ArtifactVersion = ShaderArtifactVersion;
		outArtifact.Payload = writer.TakeData();
		return ResultEnvelope::Success("asset.shader_artifact.encode", "asset:shader", "Shader artifact V2 encoded");
	}

	ResultEnvelope DecodeShaderArtifactV2(const AssetArtifact& artifact, ShaderArtifactDataV2& outShader) {
		outShader = {};
		if (artifact.Kind != AssetKind::Shader || artifact.ArtifactVersion != ShaderArtifactVersion) return MakeShaderArtifactFailure("asset.shader_artifact.decode", "asset.shader_artifact.version_mismatch", "Shader artifact kind or version is unsupported");
		AssetBinaryReaderLimits limits;
		limits.MaxStringBytes = 16 * 1024 * 1024;
		limits.MaxBlobBytes = 128 * 1024 * 1024;
		AssetBinaryReader reader(artifact.Payload, limits);
		uint32_t schema = 0, count = 0;
		std::vector<uint8_t> digest;
		Sha256Digest storedDigest{};
		uint64_t storedSignature = 0;
		Sha256Digest storedAuthoringDigest{};
		uint64_t storedAuthoringSignature = 0;
		if (!reader.ReadU32(schema) || schema != 1 || !reader.ReadString(outShader.SourceLanguage) || outShader.SourceLanguage != "HLSL" || !reader.ReadString(outShader.CompilerIdentity) || !reader.ReadU32(count) || count > 128) goto invalid;
		outShader.CompileOptions.resize(count);
		for (auto& option : outShader.CompileOptions) if (!reader.ReadString(option)) goto invalid;
		if (!reader.ReadU32(count) || count != 2) goto invalid;
		outShader.Stages.resize(count);
		for (auto& stage : outShader.Stages) {
			uint8_t stageValue = 0;
			uint32_t wordCount = 0;
			if (!reader.ReadU8(stageValue) || stageValue > 1 || !reader.ReadString(stage.EntryPoint) || !reader.ReadString(stage.Profile) || !reader.ReadU32(wordCount) || wordCount < 5 || wordCount > 16 * 1024 * 1024) goto invalid;
			stage.Stage = static_cast<Rendering::ShaderStage>(stageValue);
			stage.Spirv.resize(wordCount);
			for (auto& word : stage.Spirv) if (!reader.ReadU32(word)) goto invalid;
			if (stage.Spirv.front() != 0x07230203u || !reader.ReadString(stage.GeneratedOpenGlGlsl)) goto invalid;
		}
		if (!reader.ReadBytes(outShader.Interface.Gpu.Digest.size(), digest) || !reader.ReadU64(outShader.Interface.Gpu.Signature)) goto invalid;
		std::copy(digest.begin(), digest.end(), outShader.Interface.Gpu.Digest.begin());
		storedDigest = outShader.Interface.Gpu.Digest;
		storedSignature = outShader.Interface.Gpu.Signature;
		if (!reader.ReadU32(count) || count > 8) goto invalid;
		outShader.Interface.Gpu.Stages.resize(count);
		for (auto& stage : outShader.Interface.Gpu.Stages) {
			uint8_t value = 0; uint32_t variableCount = 0;
			if (!reader.ReadU8(value) || value > 1 || !reader.ReadString(stage.EntryPoint) || !reader.ReadU32(variableCount) || variableCount > 256) goto invalid;
			stage.Stage = static_cast<Rendering::ShaderStage>(value); stage.Inputs.resize(variableCount);
			for (auto& input : stage.Inputs) { uint8_t type = 0; if (!reader.ReadU32(input.Location) || !reader.ReadU8(type) || type > static_cast<uint8_t>(Rendering::ShaderValueType::Float4)) goto invalid; input.Type = static_cast<Rendering::ShaderValueType>(type); }
			if (!reader.ReadU32(variableCount) || variableCount > 256) goto invalid; stage.Outputs.resize(variableCount);
			for (auto& output : stage.Outputs) { uint8_t type = 0; if (!reader.ReadU32(output.Location) || !reader.ReadU8(type) || type > static_cast<uint8_t>(Rendering::ShaderValueType::Float4)) goto invalid; output.Type = static_cast<Rendering::ShaderValueType>(type); }
		}
		if (!reader.ReadU32(count) || count > 256) goto invalid;
		outShader.Interface.Gpu.VertexInputs.resize(count);
		for (auto& input : outShader.Interface.Gpu.VertexInputs) { uint8_t value = 0; if (!reader.ReadString(input.Semantic) || !reader.ReadU32(input.Location) || !reader.ReadU8(value) || value > static_cast<uint8_t>(Rendering::ShaderValueType::Float4)) goto invalid; input.Type = static_cast<Rendering::ShaderValueType>(value); }
		if (!reader.ReadU32(count) || count > 1024) goto invalid;
		outShader.Interface.Gpu.Resources.resize(count);
		for (auto& resource : outShader.Interface.Gpu.Resources) { uint8_t type = 0; if (!reader.ReadString(resource.Name) || !reader.ReadU8(type) || type > static_cast<uint8_t>(Rendering::ShaderResourceType::Sampler) || !reader.ReadU32(resource.Set) || !reader.ReadU32(resource.Binding) || !reader.ReadU32(resource.ArrayCount) || !reader.ReadU8(resource.StageMask)) goto invalid; resource.Type = static_cast<Rendering::ShaderResourceType>(type); }
		if (!reader.ReadU32(count) || count > 64) goto invalid;
		outShader.Interface.Gpu.ConstantBuffers.resize(count);
		for (auto& buffer : outShader.Interface.Gpu.ConstantBuffers) {
			uint32_t memberCount = 0;
			if (!reader.ReadString(buffer.Name) || !reader.ReadU32(buffer.Set) || !reader.ReadU32(buffer.Binding) || !reader.ReadU32(buffer.Size) || !reader.ReadU32(memberCount) || memberCount > 1024) goto invalid;
			buffer.Members.resize(memberCount);
			for (auto& member : buffer.Members) { uint8_t type = 0, columnMajor = 0; if (!reader.ReadString(member.Name) || !reader.ReadU8(type) || type > static_cast<uint8_t>(Rendering::ShaderValueType::Float4x4) || !reader.ReadU32(member.Offset) || !reader.ReadU32(member.Size) || !reader.ReadU32(member.MatrixStride) || !reader.ReadU32(member.ArrayStride) || !reader.ReadU8(columnMajor) || columnMajor > 1) goto invalid; member.Type = static_cast<Rendering::ShaderValueType>(type); member.ColumnMajor = columnMajor != 0; }
		}
		if (!reader.ReadBytes(outShader.Interface.Authoring.Digest.size(), digest) || !reader.ReadU64(outShader.Interface.Authoring.Signature)) goto invalid;
		std::copy(digest.begin(), digest.end(), outShader.Interface.Authoring.Digest.begin());
		storedAuthoringDigest = outShader.Interface.Authoring.Digest; storedAuthoringSignature = outShader.Interface.Authoring.Signature;
		if (!reader.ReadU32(count) || count > 1024) goto invalid;
		outShader.Interface.Authoring.Parameters.resize(count);
		for (auto& parameter : outShader.Interface.Authoring.Parameters) {
			uint8_t scope = 0, type = 0, editor = 0; uint32_t rangeCount = 0;
			if (!reader.ReadString(parameter.Name) || !reader.ReadString(parameter.DisplayName) || !reader.ReadU8(scope) || scope > static_cast<uint8_t>(Rendering::ShaderParameterScope::Object) || !reader.ReadU8(type) || type > static_cast<uint8_t>(Rendering::ShaderValueType::SamplerState) || !reader.ReadU8(editor) || editor > static_cast<uint8_t>(Rendering::ShaderEditorKind::Texture2D)) goto invalid;
			parameter.Scope = static_cast<Rendering::ShaderParameterScope>(scope); parameter.Type = static_cast<Rendering::ShaderValueType>(type); parameter.Editor = static_cast<Rendering::ShaderEditorKind>(editor);
			if (!ReadParameterValue(reader, parameter.Type, parameter.DefaultValue) || !reader.ReadU32(rangeCount) || rangeCount > 16) goto invalid;
			parameter.Range.resize(rangeCount); for (auto& value : parameter.Range) if (!reader.ReadFloat(value)) goto invalid;
			if (!reader.ReadFloat(parameter.Step) || !reader.ReadString(parameter.Tooltip)) goto invalid;
		}
		if (!reader.ReadU32(count) || count > 1024) goto invalid;
		outShader.OpenGlCombinedSamplers.resize(count);
		for (auto& sampler : outShader.OpenGlCombinedSamplers) if (!reader.ReadString(sampler.TextureName) || !reader.ReadString(sampler.SamplerName) || !reader.ReadString(sampler.UniformName) || sampler.TextureName.empty() || sampler.SamplerName.empty() || sampler.UniformName.empty()) goto invalid;
		if (!reader.ReadU32(count) || count > 4096) goto invalid;
		outShader.ImportInputs.resize(count);
		for (auto& input : outShader.ImportInputs) if (!reader.ReadString(input.NormalizedPath) || !reader.ReadString(input.ContentHash)) goto invalid;
		if (reader.Failed() || reader.Remaining() != 0) goto invalid;
		if (!ValidateShaderArtifactV2Contract(outShader).Succeeded() || outShader.Interface.Gpu.Digest != storedDigest || outShader.Interface.Gpu.Signature != storedSignature || outShader.Interface.Authoring.Digest != storedAuthoringDigest || outShader.Interface.Authoring.Signature != storedAuthoringSignature) goto invalid;
		return ResultEnvelope::Success("asset.shader_artifact.decode", "asset:shader", "Shader artifact V2 decoded");
	invalid:
		outShader = {};
		return MakeShaderArtifactFailure("asset.shader_artifact.decode", "asset.shader_artifact.payload_invalid", "Shader artifact V2 payload is malformed");
	}
}
