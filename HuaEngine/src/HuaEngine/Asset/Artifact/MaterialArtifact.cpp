#include "enginepch.h"
#include "MaterialArtifact.h"

#include <algorithm>
#include <limits>

#include "HuaEngine/Asset/Library/AssetBinaryIO.h"

namespace {
	constexpr uint32_t MaxMaterialParameterCount = 65536;
	constexpr uint32_t MaxMaterialArrayLength = 16 * 1024 * 1024;

	HE::ResultEnvelope MakeMaterialArtifactFailure(std::string operation, std::string code, std::string message) {
		auto result = HE::ResultEnvelope::Failure(std::move(operation), "asset:material", message);
		result.AddDetail({ HE::DiagnosticSeverity::Error, std::move(code), std::move(message), {} });
		return result;
	}

	bool EncodeValue(
		HE::AssetBinaryWriter& writer,
		const HE::Rendering::MaterialSourceParameter& parameter) {
		using namespace HE::Rendering;
		switch (parameter.Type) {
		case MaterialParameterType::Int:
			writer.WriteU32(static_cast<uint32_t>(std::get<int>(parameter.Value)));
			return true;
		case MaterialParameterType::Float:
			writer.WriteFloat(std::get<float>(parameter.Value));
			return true;
		case MaterialParameterType::Vec2:
		case MaterialParameterType::Vec3:
		case MaterialParameterType::Vec4: {
			const auto writeVector = [&](const auto& value) {
				for (glm::length_t index = 0; index < value.length(); ++index) {
					writer.WriteFloat(value[index]);
				}
			};
			if (parameter.Type == MaterialParameterType::Vec2) writeVector(std::get<glm::vec2>(parameter.Value));
			else if (parameter.Type == MaterialParameterType::Vec3) writeVector(std::get<glm::vec3>(parameter.Value));
			else writeVector(std::get<glm::vec4>(parameter.Value));
			return true;
		}
		case MaterialParameterType::Mat3: {
			const auto& value = std::get<glm::mat3>(parameter.Value);
			for (int column = 0; column < 3; ++column) for (int row = 0; row < 3; ++row) writer.WriteFloat(value[column][row]);
			return true;
		}
		case MaterialParameterType::Mat4: {
			const auto& value = std::get<glm::mat4>(parameter.Value);
			for (int column = 0; column < 4; ++column) for (int row = 0; row < 4; ++row) writer.WriteFloat(value[column][row]);
			return true;
		}
		case MaterialParameterType::Texture2D:
			writer.WriteString(std::get<std::string>(parameter.Value));
			return true;
		case MaterialParameterType::IntArray: {
			const auto& values = std::get<std::vector<int>>(parameter.Value);
			if (values.size() > std::numeric_limits<uint32_t>::max()) return false;
			writer.WriteU32(static_cast<uint32_t>(values.size()));
			for (int value : values) writer.WriteU32(static_cast<uint32_t>(value));
			return true;
		}
		case MaterialParameterType::FloatArray: {
			const auto& values = std::get<std::vector<float>>(parameter.Value);
			if (values.size() > std::numeric_limits<uint32_t>::max()) return false;
			writer.WriteU32(static_cast<uint32_t>(values.size()));
			for (float value : values) writer.WriteFloat(value);
			return true;
		}
		case MaterialParameterType::TextureCube:
			return false;
		}
		return false;
	}

	template<glm::length_t Length>
	bool ReadVector(HE::AssetBinaryReader& reader, glm::vec<Length, float>& value) {
		for (glm::length_t index = 0; index < Length; ++index) if (!reader.ReadFloat(value[index])) return false;
		return true;
	}

	bool DecodeValue(
		HE::AssetBinaryReader& reader,
		HE::Rendering::MaterialSourceParameter& parameter) {
		using namespace HE::Rendering;
		switch (parameter.Type) {
		case MaterialParameterType::Int: {
			uint32_t value = 0; if (!reader.ReadU32(value)) return false; parameter.Value = static_cast<int>(value); return true;
		}
		case MaterialParameterType::Float: {
			float value = 0; if (!reader.ReadFloat(value)) return false; parameter.Value = value; return true;
		}
		case MaterialParameterType::Vec2: { glm::vec2 value{}; if (!ReadVector(reader, value)) return false; parameter.Value = value; return true; }
		case MaterialParameterType::Vec3: { glm::vec3 value{}; if (!ReadVector(reader, value)) return false; parameter.Value = value; return true; }
		case MaterialParameterType::Vec4: { glm::vec4 value{}; if (!ReadVector(reader, value)) return false; parameter.Value = value; return true; }
		case MaterialParameterType::Mat3: {
			glm::mat3 value{}; for (int c = 0; c < 3; ++c) for (int r = 0; r < 3; ++r) if (!reader.ReadFloat(value[c][r])) return false; parameter.Value = value; return true;
		}
		case MaterialParameterType::Mat4: {
			glm::mat4 value{}; for (int c = 0; c < 4; ++c) for (int r = 0; r < 4; ++r) if (!reader.ReadFloat(value[c][r])) return false; parameter.Value = value; return true;
		}
		case MaterialParameterType::Texture2D: {
			std::string value; if (!reader.ReadString(value)) return false; parameter.Value = std::move(value); return true;
		}
		case MaterialParameterType::IntArray: {
			uint32_t count = 0; if (!reader.ReadU32(count) || count > MaxMaterialArrayLength) return false;
			std::vector<int> values(count); for (int& value : values) { uint32_t stored = 0; if (!reader.ReadU32(stored)) return false; value = static_cast<int>(stored); } parameter.Value = std::move(values); return true;
		}
		case MaterialParameterType::FloatArray: {
			uint32_t count = 0; if (!reader.ReadU32(count) || count > MaxMaterialArrayLength) return false;
			std::vector<float> values(count); for (float& value : values) if (!reader.ReadFloat(value)) return false; parameter.Value = std::move(values); return true;
		}
		case MaterialParameterType::TextureCube:
			return false;
		}
		return false;
	}
}

namespace HE {
	ResultEnvelope EncodeMaterialArtifact(
		const Rendering::MaterialSourceData& material,
		AssetArtifact& outArtifact) {
		outArtifact = {};
		if (material.Name.empty() || material.Type == Rendering::MaterialType::Empty || material.Parameters.size() > MaxMaterialParameterCount) {
			return MakeMaterialArtifactFailure("asset.material_artifact.encode", "asset.material_artifact.invalid", "Material source data is invalid");
		}

		std::vector<const Rendering::MaterialSourceParameter*> parameters;
		for (const auto& [name, parameter] : material.Parameters) { (void)name; parameters.push_back(&parameter); }
		std::sort(parameters.begin(), parameters.end(), [](const auto* left, const auto* right) { return left->Name < right->Name; });
		AssetBinaryWriter writer;
		writer.WriteString(material.Name);
		writer.WriteU32(static_cast<uint32_t>(material.Type));
		writer.WriteString(material.ShaderGuid);
		writer.WriteBytes(material.ShaderInterfaceDigest);
		writer.WriteU64(material.ShaderInterfaceSignature);
		writer.WriteBytes(material.MaterialDefinitionDigest);
		writer.WriteU64(material.MaterialDefinitionSignature);
		writer.WriteU32(static_cast<uint32_t>(parameters.size()));
		for (const auto* parameter : parameters) {
			writer.WriteString(parameter->Name);
			writer.WriteU32(static_cast<uint32_t>(parameter->Type));
			try { if (!EncodeValue(writer, *parameter)) return MakeMaterialArtifactFailure("asset.material_artifact.encode", "asset.material_artifact.parameter_unsupported", "Material parameter type is unsupported"); }
			catch (const std::bad_variant_access&) { return MakeMaterialArtifactFailure("asset.material_artifact.encode", "asset.material_artifact.parameter_mismatch", "Material parameter value does not match its type"); }
		}

		outArtifact.Kind = AssetKind::Material;
		outArtifact.ArtifactVersion = MaterialArtifactVersion;
		outArtifact.Payload = writer.TakeData();
		return ResultEnvelope::Success("asset.material_artifact.encode", material.Name, "Material artifact encoded");
	}

	ResultEnvelope DecodeMaterialArtifact(
		const AssetArtifact& artifact,
		Rendering::MaterialSourceData& outMaterial) {
		outMaterial = {};
		if (artifact.Kind != AssetKind::Material || artifact.ArtifactVersion != MaterialArtifactVersion) {
			return MakeMaterialArtifactFailure("asset.material_artifact.decode", "asset.material_artifact.version_mismatch", "Material artifact kind or version is unsupported");
		}

		AssetBinaryReader reader(artifact.Payload);
		uint32_t type = 0;
		uint32_t parameterCount = 0;
		std::vector<uint8_t> shaderDigest;
		std::vector<uint8_t> definitionDigest;
		if (!reader.ReadString(outMaterial.Name) || outMaterial.Name.empty() || !reader.ReadU32(type) ||
			type < static_cast<uint32_t>(Rendering::MaterialType::Standard) || type > static_cast<uint32_t>(Rendering::MaterialType::Custom) ||
			!reader.ReadString(outMaterial.ShaderGuid) || outMaterial.ShaderGuid.empty() ||
			!reader.ReadBytes(32, shaderDigest) || !reader.ReadU64(outMaterial.ShaderInterfaceSignature) ||
			!reader.ReadBytes(32, definitionDigest) || !reader.ReadU64(outMaterial.MaterialDefinitionSignature) ||
			!reader.ReadU32(parameterCount) || parameterCount > MaxMaterialParameterCount) {
			return MakeMaterialArtifactFailure("asset.material_artifact.decode", "asset.material_artifact.header_invalid", "Material artifact header is invalid");
		}
		std::copy(shaderDigest.begin(), shaderDigest.end(), outMaterial.ShaderInterfaceDigest.begin());
		std::copy(definitionDigest.begin(), definitionDigest.end(), outMaterial.MaterialDefinitionDigest.begin());
		outMaterial.Type = static_cast<Rendering::MaterialType>(type);

		for (uint32_t index = 0; index < parameterCount; ++index) {
			Rendering::MaterialSourceParameter parameter;
			uint32_t parameterType = 0;
			if (!reader.ReadString(parameter.Name) || parameter.Name.empty() || !reader.ReadU32(parameterType) ||
				parameterType > static_cast<uint32_t>(Rendering::MaterialParameterType::FloatArray)) {
				return MakeMaterialArtifactFailure("asset.material_artifact.decode", "asset.material_artifact.parameter_invalid", "Material artifact parameter metadata is invalid");
			}
			parameter.Type = static_cast<Rendering::MaterialParameterType>(parameterType);
			if (!DecodeValue(reader, parameter) || !outMaterial.Parameters.emplace(parameter.Name, std::move(parameter)).second) {
				return MakeMaterialArtifactFailure("asset.material_artifact.decode", "asset.material_artifact.parameter_invalid", "Material artifact parameter payload is invalid");
			}
		}

		if (reader.Failed() || reader.Remaining() != 0) {
			return MakeMaterialArtifactFailure("asset.material_artifact.decode", "asset.material_artifact.payload_invalid", "Material artifact contains malformed trailing data");
		}
		return ResultEnvelope::Success("asset.material_artifact.decode", outMaterial.Name, "Material artifact decoded");
	}
}
