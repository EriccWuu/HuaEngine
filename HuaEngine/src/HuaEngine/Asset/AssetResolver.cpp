#include "enginepch.h"
#include "AssetResolver.h"

#include "AssetService.h"
#include "HuaEngine/Rendering/Material/MaterialLibrary.h"
#include "HuaEngine/Serialization/Serialization.h"

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
		result.AddDetail({ HE::DiagnosticSeverity::Error, "asset.manifest.unloaded", "Call AssetService::LoadOrCreateManifest with a project context before resolving assets", guid });
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

		if (auto cached = m_Service->GetRuntimeCache().FindMesh(guid)) {
			outMesh = cached;
			return ResultEnvelope::Success("asset.resolve_mesh", guid, "Mesh asset resolved from runtime cache");
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

		Ref<Rendering::Mesh> mesh = nullptr;
		if (record->Source == AssetSource::Builtin) {
			mesh = CreateBuiltinMesh(AssetManifestRecord{
				.Guid = record->Guid,
				.AssetId = record->AssetId,
				.Kind = record->Kind,
				.Source = record->Source,
				.RelativePath = record->RelativePath,
				.BuiltinName = record->BuiltinName,
				.ImportState = record->ImportState
			});
		}
		else if (record->Source == AssetSource::File) {
			mesh = Rendering::Mesh::LoadFromFile(record->AbsolutePath.generic_string());
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

		if (auto cached = m_Service->GetRuntimeCache().FindMaterial(guid)) {
			outMaterial = cached;
			return ResultEnvelope::Success("asset.resolve_material", guid, "Material asset resolved from runtime cache");
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

		Ref<Rendering::Material> material = nullptr;
		if (record->Source == AssetSource::Builtin) {
			material = CreateBuiltinMaterial(AssetManifestRecord{
				.Guid = record->Guid,
				.AssetId = record->AssetId,
				.Kind = record->Kind,
				.Source = record->Source,
				.RelativePath = record->RelativePath,
				.BuiltinName = record->BuiltinName,
				.ImportState = record->ImportState
			});
		}
		else if (record->Source == AssetSource::File) {
			material = Rendering::Material::CreateFromDeserialization();
			if (!material || !Serialization::LoadMaterial(record->AbsolutePath.generic_string(), *material)) {
				material = nullptr;
			}
		}

		if (!material) {
			auto result = ResultEnvelope::ManualIntervention("asset.resolve_material", guid, "Material asset could not be loaded");
			result.AddDetail({ DiagnosticSeverity::Warning, "asset.material.load_failed", "Material runtime object could not be created from metadata", record->AssetId });
			return result;
		}

		m_Service->GetRuntimeCache().StoreMaterial(guid, material);
		outMaterial = material;
		return ResultEnvelope::Success("asset.resolve_material", guid, "Material asset resolved");
	}

	ResultEnvelope AssetResolver::ResolveTexture(const AssetGuid& guid, Ref<Rendering::Texture2D>& outTexture) {
		outTexture = nullptr;
		if (guid.empty()) {
			return ResultEnvelope::Failure("asset.resolve_texture", {}, "Texture asset guid is empty");
		}

		if (auto cached = m_Service->GetRuntimeCache().FindTexture(guid)) {
			outTexture = cached;
			return ResultEnvelope::Success("asset.resolve_texture", guid, "Texture asset resolved from runtime cache");
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

		if (record->Source == AssetSource::File) {
			auto result = ResultEnvelope::ManualIntervention("asset.resolve_texture", guid, "Texture file loader is not supported by AssetResolver");
			result.AddDetail({ DiagnosticSeverity::Warning, "asset.texture.loader_unsupported", "Texture metadata was found, but source=file loading is not enabled in this resolver path", record->AssetId });
			return result;
		}
		if (record->Source == AssetSource::Builtin) {
			auto result = ResultEnvelope::ManualIntervention("asset.resolve_texture", guid, "Builtin texture loading is not supported by AssetResolver");
			result.AddDetail({ DiagnosticSeverity::Warning, "asset.texture.builtin_unsupported", "Texture metadata was found, but source=builtin has no runtime factory in this resolver path", record->AssetId });
			return result;
		}

		auto result = ResultEnvelope::ManualIntervention("asset.resolve_texture", guid, "Texture asset source is unsupported");
		result.AddDetail({ DiagnosticSeverity::Warning, "asset.texture.source_unsupported", "Texture metadata was found, but its source type cannot create a runtime texture", record->AssetId });
		return result;
	}

	Ref<Rendering::Mesh> AssetResolver::CreateBuiltinMesh(const AssetManifestRecord& record) const {
		if (record.BuiltinName == "quad") {
			return Rendering::Mesh::CreateQuad(record.AssetId);
		}
		if (record.BuiltinName == "cube" || record.BuiltinName == "fallback") {
			return Rendering::Mesh::CreateCube(record.AssetId);
		}
		if (record.BuiltinName == "sphere") {
			return Rendering::Mesh::CreateSphere(record.AssetId);
		}
		return nullptr;
	}

	Ref<Rendering::Material> AssetResolver::CreateBuiltinMaterial(const AssetManifestRecord& record) const {
		auto& library = Rendering::MaterialLibrary::Instance();
		library.CreateDefaultMaterials();

		if (record.BuiltinName == "default") {
			return library.GetDefaultMaterial();
		}
		if (record.BuiltinName == "fallback") {
			if (library.HasMaterial(record.AssetId)) {
				return library.GetMaterial(record.AssetId);
			}

			auto fallback = Rendering::Material::Create(record.AssetId, Rendering::MaterialType::Unlit);
			if (fallback) {
				library.RegisterMaterial(record.AssetId, fallback);
			}
			return fallback;
		}
		return nullptr;
	}
}
