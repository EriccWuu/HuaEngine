#include "enginepch.h"
#include "AssetResolver.h"

#include "HuaEngine/Asset/Artifact/MeshArtifact.h"
#include "HuaEngine/Asset/Artifact/MaterialArtifact.h"
#include "HuaEngine/Asset/Artifact/TextureArtifact.h"

#include "AssetService.h"
#include "HuaEngine/Rendering/Material/MaterialLibrary.h"
#include "HuaEngine/Rendering/RHI/RenderHardwareInterface.h"
#include "HuaEngine/Rendering/RHI/ShaderProgramLoader.h"

namespace {
	HE::ResultEnvelope MakeKindMismatchResult(std::string operation, const HE::AssetGuid& guid, HE::AssetKind expected, HE::AssetKind actual) {
		auto result = HE::ResultEnvelope::Failure(std::move(operation), guid, "Asset metadata kind does not match requested runtime type");
		result.AddDetail({ HE::DiagnosticSeverity::Error, "asset.kind.mismatch", "Requested asset kind does not match metadata", std::string(HE::ToString(actual)) });
		result.SetPayloadValue("expected_kind", std::string(HE::ToString(expected)));
		result.SetPayloadValue("actual_kind", std::string(HE::ToString(actual)));
		return result;
	}

	HE::ResultEnvelope MakeManifestUnloadedResult(std::string operation, const HE::AssetGuid& guid) {
		auto result = HE::ResultEnvelope::Failure(std::move(operation), guid, "Asset manifest is not loaded");
		result.AddDetail({ HE::DiagnosticSeverity::Error, "asset.manifest.unloaded", "Call AssetService::InitializeProjectAssets with a project context before resolving assets", guid });
		return result;
	}

	HE::ResultEnvelope MakeUnsupportedSourceResult(std::string operation, const HE::AssetGuid& guid, HE::AssetSource source) {
		auto result = HE::ResultEnvelope::ManualIntervention(std::move(operation), guid, "Asset source is not supported by this resolver");
		result.AddDetail({ HE::DiagnosticSeverity::Warning, "asset.source.unsupported", "Asset metadata source cannot create this runtime type", std::string(HE::ToString(source)) });
		return result;
	}

}

namespace HE {
	AssetResolver::AssetResolver(AssetService& service)
		: m_Service(&service) {}

	ResultEnvelope AssetResolver::ResolveMesh(const AssetGuid& guid, Ref<Rendering::Mesh>& outMesh) {
		outMesh = nullptr;
		if (guid.empty()) {
			return ResultEnvelope::Failure("asset.resolve_mesh", {}, "Mesh asset guid is empty");
		}

		if (!m_Service->IsManifestLoaded()) {
			return MakeManifestUnloadedResult("asset.resolve_mesh", guid);
		}

		const auto* record = m_Service->FindRecordByGuid(guid);
		if (!record) {
			return ResultEnvelope::Failure("asset.resolve_mesh", guid, "Mesh asset metadata was not found");
		}
		if (record->Kind != AssetKind::Mesh) {
			return MakeKindMismatchResult("asset.resolve_mesh", guid, AssetKind::Mesh, record->Kind);
		}
		if (record->Source != AssetSource::Builtin && record->Source != AssetSource::File) {
			return MakeUnsupportedSourceResult("asset.resolve_mesh", guid, record->Source);
		}

		if (auto cached = m_Service->GetRuntimeCache().FindMesh(guid)) {
			outMesh = cached;
			return ResultEnvelope::Success("asset.resolve_mesh", guid, "Mesh asset resolved from runtime cache");
		}

		AssetArtifact artifact;
		auto readResult = m_Service->GetLibrary().ReadArtifact(guid, artifact);
		if (!readResult.Succeeded()) {
			auto result = ResultEnvelope::ManualIntervention("asset.resolve_mesh", guid, "Mesh artifact is unavailable");
			result.AddDetail({ DiagnosticSeverity::Warning, "asset.mesh.artifact_unavailable", readResult.Summary, record->AssetId });
			return result;
		}

		Ref<Rendering::Mesh> mesh;
		auto decodeResult = DecodeMeshArtifact(artifact, mesh);
		if (!decodeResult.Succeeded()) {
			auto result = ResultEnvelope::ManualIntervention("asset.resolve_mesh", guid, "Mesh artifact could not be decoded");
			result.AddDetail({ DiagnosticSeverity::Warning, "asset.mesh.artifact_decode_failed", decodeResult.Summary, record->AssetId });
			return result;
		}

		if (!mesh) {
			auto result = ResultEnvelope::ManualIntervention("asset.resolve_mesh", guid, "Mesh asset could not be loaded");
			result.AddDetail({ DiagnosticSeverity::Warning, "asset.mesh.load_failed", "Mesh runtime object could not be created from metadata", record->AssetId });
			return result;
		}

		m_Service->GetRuntimeCache().StoreMesh(guid, mesh);
		outMesh = mesh;
		return ResultEnvelope::Success("asset.resolve_mesh", guid, "Mesh asset resolved");
	}

	ResultEnvelope AssetResolver::ResolveMaterial(const AssetGuid& guid, Ref<Rendering::Material>& outMaterial) {
		outMaterial = nullptr;
		if (guid.empty()) {
			return ResultEnvelope::Failure("asset.resolve_material", {}, "Material asset guid is empty");
		}

		if (!m_Service->IsManifestLoaded()) {
			return MakeManifestUnloadedResult("asset.resolve_material", guid);
		}

		const auto* record = m_Service->FindRecordByGuid(guid);
		if (!record) {
			return ResultEnvelope::Failure("asset.resolve_material", guid, "Material asset metadata was not found");
		}
		if (record->Kind != AssetKind::Material) {
			return MakeKindMismatchResult("asset.resolve_material", guid, AssetKind::Material, record->Kind);
		}
		if (record->Source != AssetSource::Builtin && record->Source != AssetSource::File) {
			return MakeUnsupportedSourceResult("asset.resolve_material", guid, record->Source);
		}

		if (auto cached = m_Service->GetRuntimeCache().FindMaterial(guid)) {
			outMaterial = cached;
			return ResultEnvelope::Success("asset.resolve_material", guid, "Material asset resolved from runtime cache");
		}

		Ref<Rendering::Material> material = nullptr;
		std::vector<DiagnosticEntry> materialDiagnostics;
		AssetArtifact artifact;
		auto readResult = m_Service->GetLibrary().ReadArtifact(guid, artifact);
		if (!readResult.Succeeded()) {
			auto result = ResultEnvelope::ManualIntervention("asset.resolve_material", guid, "Material artifact is unavailable");
			result.AddDetail({ DiagnosticSeverity::Warning, "asset.material.artifact_unavailable", readResult.Summary, record->AssetId });
			return result;
		}

		Rendering::MaterialSourceData sourceData;
		auto decodeResult = DecodeMaterialArtifact(artifact, sourceData);
		if (!decodeResult.Succeeded()) {
			auto result = ResultEnvelope::ManualIntervention("asset.resolve_material", guid, "Material artifact could not be decoded");
			result.AddDetail({ DiagnosticSeverity::Warning, "asset.material.artifact_decode_failed", decodeResult.Summary, record->AssetId });
			return result;
		}

		material = Rendering::Material::Create(sourceData.Name, sourceData.Type);
		if (!material) {
			auto result = ResultEnvelope::ManualIntervention("asset.resolve_material", guid, "Material asset could not be loaded");
			result.AddDetail({ DiagnosticSeverity::Warning, "asset.material.load_failed", "Material runtime object could not be created from metadata", record->AssetId });
			return result;
		}
		if (!sourceData.ShaderPath.empty() && Rendering::RenderHardwareInterface::IsInitialized()) {
			auto shader = Rendering::ShaderProgramLoader::CreateFromFile(sourceData.ShaderPath);
			material->SetShaderProgram(shader, sourceData.ShaderPath);
		}
		for (const auto& [name, sourceParameter] : sourceData.Parameters) {
			Rendering::MaterialParameterValue value;
			if (sourceParameter.Type == Rendering::MaterialParameterType::Texture2D) {
				Ref<Rendering::TextureResource> texture;
				const auto& textureGuid = std::get<std::string>(sourceParameter.Value);
				if (!textureGuid.empty()) {
					auto textureResult = ResolveTexture(textureGuid, texture);
					if (!textureResult.Succeeded()) {
						materialDiagnostics.push_back({
							DiagnosticSeverity::Warning,
							"asset.material.texture_unresolved",
							textureResult.Summary,
							textureGuid
						});
					}
				}
				value = std::move(texture);
			}
			else {
				std::visit([&](const auto& sourceValue) {
					using ValueType = std::decay_t<decltype(sourceValue)>;
					if constexpr (!std::is_same_v<ValueType, std::string>) value = sourceValue;
				}, sourceParameter.Value);
			}
			material->AddParameter({ name, sourceParameter.Type, std::move(value) });
		}
		for (const auto& [name, slot] : sourceData.TextureSlots) material->SetTextureSlot(name, slot);
		Rendering::MaterialLibrary::Instance().RegisterMaterial(record->AssetId, material);
		if (material->GetName() != record->AssetId) Rendering::MaterialLibrary::Instance().RegisterMaterial(material->GetName(), material);

		m_Service->GetRuntimeCache().StoreMaterial(guid, material);
		outMaterial = material;
		auto result = ResultEnvelope::Success("asset.resolve_material", guid, "Material asset resolved");
		for (auto& diagnostic : materialDiagnostics) result.AddDetail(std::move(diagnostic));
		return result;
	}

	ResultEnvelope AssetResolver::ResolveTexture(const AssetGuid& guid, Ref<Rendering::TextureResource>& outTexture) {
		outTexture = nullptr;
		if (guid.empty()) {
			return ResultEnvelope::Failure("asset.resolve_texture", {}, "Texture asset guid is empty");
		}

		if (!m_Service->IsManifestLoaded()) {
			return MakeManifestUnloadedResult("asset.resolve_texture", guid);
		}

		const auto* record = m_Service->FindRecordByGuid(guid);
		if (!record) {
			return ResultEnvelope::Failure("asset.resolve_texture", guid, "Texture asset metadata was not found");
		}
		if (record->Kind != AssetKind::Texture2D) {
			return MakeKindMismatchResult("asset.resolve_texture", guid, AssetKind::Texture2D, record->Kind);
		}
		if (record->Source != AssetSource::Builtin && record->Source != AssetSource::File) {
			return MakeUnsupportedSourceResult("asset.resolve_texture", guid, record->Source);
		}

		if (auto cached = m_Service->GetRuntimeCache().FindTexture(guid)) {
			outTexture = cached;
			return ResultEnvelope::Success("asset.resolve_texture", guid, "Texture asset resolved from runtime cache");
		}

		AssetArtifact artifact;
		auto readResult = m_Service->GetLibrary().ReadArtifact(guid, artifact);
		if (!readResult.Succeeded()) {
			auto result = ResultEnvelope::ManualIntervention("asset.resolve_texture", guid, "Texture artifact is unavailable");
			result.AddDetail({ DiagnosticSeverity::Warning, "asset.texture.artifact_unavailable", readResult.Summary, record->AssetId });
			return result;
		}

		TextureArtifactData textureData;
		auto decodeResult = DecodeTextureArtifact(artifact, textureData);
		if (!decodeResult.Succeeded()) {
			auto result = ResultEnvelope::ManualIntervention("asset.resolve_texture", guid, "Texture artifact could not be decoded");
			result.AddDetail({ DiagnosticSeverity::Warning, "asset.texture.artifact_decode_failed", decodeResult.Summary, record->AssetId });
			return result;
		}

		auto& device = Rendering::RenderHardwareInterface::GetDevice();
		auto texture = device.CreateTexture({
			.Width = textureData.Width,
			.Height = textureData.Height,
			.Format = Rendering::RenderTargetTextureFormat::RGBA8,
			.Usage = Rendering::TextureUsageSampled | Rendering::TextureUsageCopyDst,
			.MipLevels = textureData.MipLevels,
			.Samples = 1
		});
		if (!texture || !device.UploadTexture({ .Texture = texture, .MipLevel = 0, .Data = std::move(textureData.Pixels) })) {
			auto result = ResultEnvelope::ManualIntervention("asset.resolve_texture", guid, "Texture artifact could not be uploaded");
			result.AddDetail({ DiagnosticSeverity::Warning, "asset.texture.upload_failed", "RenderDevice rejected the texture description or pixel payload", record->AssetId });
			return result;
		}

		m_Service->GetRuntimeCache().StoreTexture(guid, texture);
		outTexture = texture;
		return ResultEnvelope::Success("asset.resolve_texture", guid, "Texture asset resolved");
	}
}
