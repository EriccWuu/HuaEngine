#include "enginepch.h"
#include "MaterialAssetImporter.h"

#include <algorithm>

#include "HuaEngine/Asset/Artifact/ShaderArtifact.h"
#include "HuaEngine/Asset/Library/AssetLibrary.h"
#include "HuaEngine/Core/Sha256.h"

namespace {
	bool ToMaterialType(HE::Rendering::ShaderValueType type, HE::Rendering::MaterialParameterType& output) {
		using namespace HE::Rendering;
		switch (type) {
		case ShaderValueType::Int: output = MaterialParameterType::Int; return true;
		case ShaderValueType::Float: output = MaterialParameterType::Float; return true;
		case ShaderValueType::Float2: output = MaterialParameterType::Vec2; return true;
		case ShaderValueType::Float3: output = MaterialParameterType::Vec3; return true;
		case ShaderValueType::Float4: output = MaterialParameterType::Vec4; return true;
		case ShaderValueType::Float4x4: output = MaterialParameterType::Mat4; return true;
		case ShaderValueType::Texture2D: output = MaterialParameterType::Texture2D; return true;
		case ShaderValueType::SamplerState: return false;
		}
		return false;
	}

	bool LoadShaderInterface(const HE::AssetImportContext& context, const HE::AssetGuid& guid, HE::ShaderArtifactDataV2& output) {
		if (!context.Library) return false;
		HE::AssetArtifact artifact;
		return context.Library->ReadArtifact(guid, artifact).Succeeded() && HE::DecodeShaderArtifactV2(artifact, output).Succeeded();
	}

	bool ApplyShaderSchema(HE::Rendering::MaterialSourceData& material, const HE::ShaderArtifactDataV2& shader) {
		using namespace HE::Rendering;
		for (const auto& [name, parameter] : material.Parameters) {
			const auto metadata = std::find_if(shader.Interface.Authoring.Parameters.begin(), shader.Interface.Authoring.Parameters.end(), [&](const auto& value) { return value.Name == name && value.Scope == ShaderParameterScope::Material; });
			MaterialParameterType expected;
			if (metadata == shader.Interface.Authoring.Parameters.end() || !ToMaterialType(metadata->Type, expected) || expected != parameter.Type) return false;
		}
		for (const auto& metadata : shader.Interface.Authoring.Parameters) {
			if (metadata.Scope != ShaderParameterScope::Material || material.Parameters.contains(metadata.Name)) continue;
			MaterialParameterType type;
			if (!ToMaterialType(metadata.Type, type)) continue;
			MaterialSourceParameter parameter{ .Name = metadata.Name, .Type = type };
			std::visit([&](const auto& value) { parameter.Value = value; }, metadata.DefaultValue);
			material.Parameters.emplace(parameter.Name, std::move(parameter));
		}
		material.ShaderInterfaceDigest = shader.Interface.Gpu.Digest;
		material.ShaderInterfaceSignature = shader.Interface.Gpu.Signature;
		material.MaterialDefinitionDigest = shader.Interface.Authoring.Digest;
		material.MaterialDefinitionSignature = shader.Interface.Authoring.Signature;
		return true;
	}
}

namespace HE {
	bool MaterialAssetImporter::CanImport(AssetKind kind, std::string_view extension) const {
		return kind == AssetKind::Material && (extension == ".material" || extension == ".mat");
	}

	ResultEnvelope MaterialAssetImporter::BuildFingerprintInput(const AssetImportContext& context, std::string_view rootSourceHash, AssetImportFingerprintInput& output) const {
		Rendering::MaterialSourceData source;
		auto load = Rendering::LoadMaterialSourceData(context.SourcePath, source);
		if (!load.Succeeded()) return load;
		ShaderArtifactDataV2 shader;
		if (!LoadShaderInterface(context, source.ShaderGuid, shader)) return ResultEnvelope::Failure("asset.import_fingerprint.inputs", source.ShaderGuid, "Material shader artifact is unavailable");
		output = { .ImporterId = std::string(GetId()), .ImporterVersion = GetVersion(), .ArtifactVersion = GetArtifactVersion(), .Sources = { { "source", std::string(rootSourceHash) } } };
		output.Dependencies.push_back({ source.ShaderGuid, Sha256ToHex(shader.Interface.Gpu.Digest) });
		for (const auto& [name, parameter] : source.Parameters) {
			(void)name;
			if (parameter.Type != Rendering::MaterialParameterType::Texture2D) continue;
			const auto& reference = std::get<std::string>(parameter.Value);
			const auto* record = context.Manifest ? context.Manifest->FindByAssetId(reference) : nullptr;
			if (record) {
				const auto bytes = std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(record->Guid.data()), record->Guid.size());
				output.Dependencies.push_back({ record->Guid, Sha256ToHex(ComputeSha256(bytes)) });
			}
		}
		return ResultEnvelope::Success("asset.import_fingerprint.inputs", context.SourceAsset.Guid, "Material fingerprint inputs collected");
	}

	AssetImportResult MaterialAssetImporter::Import(const AssetImportContext& context) const {
		AssetImportResult result;
		std::vector<AssetGuid> dependencies;
		Rendering::MaterialSourceData sourceData;
		auto loadResult = Rendering::LoadMaterialSourceData(context.SourcePath, sourceData);
		if (!loadResult.Succeeded()) {
			result.Diagnostics = std::move(loadResult.Details);
			return result;
		}
		if (!sourceData.ShaderGuid.empty()) {
			const auto* shaderRecord = context.Manifest ? context.Manifest->FindByGuid(sourceData.ShaderGuid) : nullptr;
			if (!shaderRecord || shaderRecord->Kind != AssetKind::Shader) {
				result.Diagnostics.push_back({
					DiagnosticSeverity::Error,
					"asset.import.material_shader_unresolved",
					"Material shader GUID must reference a registered shader asset",
					sourceData.ShaderGuid
				});
				return result;
			}
			dependencies.push_back(shaderRecord->Guid);
		}
		ShaderArtifactDataV2 shaderData;
		if (context.Library) {
			if (!LoadShaderInterface(context, sourceData.ShaderGuid, shaderData) || !ApplyShaderSchema(sourceData, shaderData)) {
				result.Diagnostics.push_back({ DiagnosticSeverity::Error, "asset.import.material_schema_mismatch", "Material parameters do not match the shader interface", sourceData.ShaderGuid });
				return result;
			}
		}
		else {
			const auto bytes = std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(sourceData.ShaderGuid.data()), sourceData.ShaderGuid.size());
			sourceData.ShaderInterfaceDigest = ComputeSha256(bytes);
			sourceData.ShaderInterfaceSignature = Sha256Prefix64(sourceData.ShaderInterfaceDigest);
			sourceData.MaterialDefinitionDigest = sourceData.ShaderInterfaceDigest;
			sourceData.MaterialDefinitionSignature = sourceData.ShaderInterfaceSignature;
		}

		for (auto& [name, parameter] : sourceData.Parameters) {
			(void)name;
			if (parameter.Type != Rendering::MaterialParameterType::Texture2D) continue;
			auto& textureReference = std::get<std::string>(parameter.Value);
			if (textureReference.empty()) continue;

			const auto* textureRecord = context.Manifest ? context.Manifest->FindByAssetId(textureReference) : nullptr;
			if (!textureRecord || textureRecord->Kind != AssetKind::Texture2D) {
				result.Diagnostics.push_back({
					DiagnosticSeverity::Error,
					"asset.import.material_texture_unresolved",
					"Material texture reference must identify a registered texture asset",
					textureReference
				});
				return result;
			}

			textureReference = textureRecord->Guid;
			dependencies.push_back(textureRecord->Guid);
		}

		auto encodeResult = EncodeMaterialArtifact(sourceData, result.Artifact);
		if (!encodeResult.Succeeded()) {
			result.Diagnostics.insert(result.Diagnostics.end(), encodeResult.Details.begin(), encodeResult.Details.end());
			return result;
		}
		std::sort(dependencies.begin(), dependencies.end());
		result.Artifact.Dependencies = std::move(dependencies);
		result.Artifact.Dependencies.erase(
			std::unique(result.Artifact.Dependencies.begin(), result.Artifact.Dependencies.end()),
			result.Artifact.Dependencies.end());
		result.Success = true;
		return result;
	}
}
