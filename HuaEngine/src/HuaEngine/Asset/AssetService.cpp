#include "enginepch.h"
#include "AssetService.h"

#include <system_error>

#include "AssetResolver.h"
#include "HuaEngine/Rendering/Material/MaterialLibrary.h"
#include "HuaEngine/Rendering/Material/MaterialSerializer.h"
#include "HuaEngine/Rendering/Mesh/MeshManager.h"
#include "HuaEngine/Serialization/Serialization.h"

namespace {
	struct NormalizedAssetPath {
		std::string AssetId;
		std::filesystem::path RelativePath;
		std::filesystem::path AbsolutePath;
		bool ExistsOnDisk = false;
	};

	std::string HandleToString(HE::AssetHandle handle) {
		return std::to_string(handle);
	}

	std::string CountToString(uint32_t value) {
		return std::to_string(value);
	}

	bool IsEscapingAssetRoot(const std::filesystem::path& relativePath) {
		const auto normalized = relativePath.generic_string();
		return normalized == ".." || normalized.rfind("../", 0) == 0 || normalized.find("/../") != std::string::npos;
	}

	bool IsOutsideAssetRoot(const std::filesystem::path& assetRoot, const std::filesystem::path& absolutePath) {
		if (assetRoot.empty() || absolutePath.empty()) {
			return true;
		}

		const auto relativePath = absolutePath.lexically_relative(assetRoot);
		if (relativePath.empty() && absolutePath != assetRoot) {
			return true;
		}

		return relativePath.is_absolute() || IsEscapingAssetRoot(relativePath);
	}

	bool TryNormalizeAssetPath(
		const HE::ProjectContext& context,
		std::string_view assetId,
		NormalizedAssetPath& outPath,
		HE::ResultEnvelope& outError) {
		if (!context.IsLoaded()) {
			outError = HE::ResultEnvelope::Failure("asset.path.normalize", std::string(assetId), "Project context is not loaded");
			outError.AddDetail({ HE::DiagnosticSeverity::Error, "asset.project.unloaded", "Asset operations require a loaded project context", {} });
			return false;
		}

		if (assetId.empty()) {
			outError = HE::ResultEnvelope::Failure("asset.path.normalize", {}, "Asset id is empty");
			outError.AddDetail({ HE::DiagnosticSeverity::Error, "asset.id.empty", "Asset id must be a relative path under the project asset root", {} });
			return false;
		}

		std::error_code errorCode;
		const auto assetRoot = std::filesystem::absolute(context.GetAssetRootPath(), errorCode).lexically_normal();
		if (errorCode) {
			outError = HE::ResultEnvelope::Failure("asset.path.normalize", std::string(assetId), "Failed to resolve project asset root");
			outError.AddDetail({ HE::DiagnosticSeverity::Error, "asset.root.resolve_failed", errorCode.message(), context.GetAssetRootPath().generic_string() });
			return false;
		}

		std::filesystem::path inputPath(assetId);
		std::filesystem::path relativePath;
		if (inputPath.is_absolute()) {
			relativePath = std::filesystem::relative(inputPath, assetRoot, errorCode);
			if (errorCode) {
				outError = HE::ResultEnvelope::Failure("asset.path.normalize", inputPath.generic_string(), "Asset path is outside the project asset root");
				outError.AddDetail({ HE::DiagnosticSeverity::Error, "asset.path.outside_root", errorCode.message(), inputPath.generic_string() });
				return false;
			}
		}
		else {
			relativePath = inputPath;
		}

		relativePath = relativePath.lexically_normal();
		if (relativePath.empty() || relativePath == ".") {
			outError = HE::ResultEnvelope::Failure("asset.path.normalize", std::string(assetId), "Asset id normalized to an empty path");
			outError.AddDetail({ HE::DiagnosticSeverity::Error, "asset.id.invalid", "Asset id must not normalize to the asset root itself", std::string(assetId) });
			return false;
		}

		if (relativePath.is_absolute() || IsEscapingAssetRoot(relativePath)) {
			outError = HE::ResultEnvelope::Failure("asset.path.normalize", std::string(assetId), "Asset id escapes the project asset root");
			outError.AddDetail({ HE::DiagnosticSeverity::Error, "asset.id.escapes_root", "Asset id must remain within the asset root", relativePath.generic_string() });
			return false;
		}

		outPath.RelativePath = relativePath;
		outPath.AssetId = relativePath.generic_string();
		outPath.AbsolutePath = (assetRoot / relativePath).lexically_normal();
		outPath.ExistsOnDisk = std::filesystem::exists(outPath.AbsolutePath, errorCode);
		return true;
	}

	HE::ResultEnvelope MakeRegistrationResult(
		std::string operation,
		const HE::AssetRecord& record,
		std::string summary) {
		auto result = HE::ResultEnvelope::Success(std::move(operation), record.AssetId, std::move(summary));
		result.SetPayloadValue("asset_handle", HandleToString(record.Handle));
		result.SetPayloadValue("asset_guid", record.Guid);
		result.SetPayloadValue("asset_id", record.AssetId);
		result.SetPayloadValue("asset_kind", std::string(HE::ToString(record.Kind)));
		result.SetPayloadValue("asset_source", std::string(HE::ToString(record.Source)));
		result.SetPayloadValue("import_state", std::string(HE::ToString(record.ImportState)));
		result.SetPayloadValue("asset_path", record.AbsolutePath.generic_string());
		result.SetPayloadValue("exists_on_disk", record.ExistsOnDisk ? "true" : "false");
		return result;
	}

	HE::AssetGuid GetExistingGuidOrGenerate(const HE::AssetRegistry& registry, const HE::AssetManifest& manifest, std::string_view assetId) {
		if (const auto* existing = manifest.FindByAssetId(assetId)) {
			if (!existing->Guid.empty()) {
				return existing->Guid;
			}
		}
		if (const auto* existing = registry.Find(assetId)) {
			if (!existing->Guid.empty()) {
				return existing->Guid;
			}
		}

		return HE::GenerateAssetGuid();
	}

	HE::ResultEnvelope MakeResolveFailure(std::string operation, std::string target, std::string summary) {
		auto result = HE::ResultEnvelope::Failure(std::move(operation), std::move(target), std::move(summary));
		result.AddDetail({ HE::DiagnosticSeverity::Error, "asset.lookup.missing", "Requested asset could not be found in the registry", {} });
		return result;
	}

	HE::AssetRecord MakeRegistryRecord(
		const HE::ProjectContext& context,
		const HE::AssetManifestRecord& manifestRecord,
		HE::AssetHandle handle = 0) {
		HE::AssetRecord record;
		record.Handle = handle;
		record.Guid = manifestRecord.Guid;
		record.Kind = manifestRecord.Kind;
		record.Source = manifestRecord.Source;
		record.AssetId = manifestRecord.AssetId;
		record.RelativePath = manifestRecord.RelativePath;
		record.BuiltinName = manifestRecord.BuiltinName;
		record.ImportState = manifestRecord.ImportState;
		if (record.Source == HE::AssetSource::File) {
			record.AbsolutePath = (context.GetAssetRootPath() / record.RelativePath).lexically_normal();
			std::error_code errorCode;
			record.ExistsOnDisk = std::filesystem::is_regular_file(record.AbsolutePath, errorCode);
		}
		return record;
	}

	HE::Ref<HE::Rendering::Mesh> CreateBuiltinMesh(
		HE::BuiltinMeshPrimitive primitive,
		std::string_view meshName) {
		const std::string resolvedMeshName = meshName.empty()
			? std::string(HE::ToString(primitive))
			: std::string(meshName);

		switch (primitive) {
		case HE::BuiltinMeshPrimitive::Quad:
			return HE::Rendering::Mesh::CreateQuad(resolvedMeshName);
		case HE::BuiltinMeshPrimitive::Cube:
			return HE::Rendering::Mesh::CreateCube(resolvedMeshName);
		case HE::BuiltinMeshPrimitive::Sphere:
			return HE::Rendering::Mesh::CreateSphere(resolvedMeshName);
		}

		return nullptr;
	}
}

namespace HE {
	ResultEnvelope AssetService::LoadOrCreateManifest(const ProjectContext& context) {
		AssetManifest loadedManifest;
		auto result = LoadOrCreateAssetManifest(context, loadedManifest);
		if (!result.Succeeded()) {
			return result;
		}

		m_Registry = AssetRegistry();
		m_RuntimeCache = AssetRuntimeCache();
		m_Manifest = std::move(loadedManifest);
		m_Manifest.ForEachRecord([&](const AssetManifestRecord& manifestRecord) {
			(void)m_Registry.Upsert(MakeRegistryRecord(context, manifestRecord));
		});
		return result;
	}

	ResultEnvelope AssetService::CreateBuiltinMeshAsset(
		const ProjectContext& context,
		std::string_view assetId,
		BuiltinMeshPrimitive primitive,
		std::string_view meshName,
		AssetHandle* outHandle) {
		NormalizedAssetPath normalizedPath;
		ResultEnvelope normalizeError;
		if (!TryNormalizeAssetPath(context, assetId, normalizedPath, normalizeError)) {
			normalizeError.Operation = "asset.create_builtin_mesh";
			return normalizeError;
		}

		auto mesh = CreateBuiltinMesh(primitive, meshName);
		if (!mesh) {
			auto result = ResultEnvelope::Failure("asset.create_builtin_mesh", normalizedPath.AssetId, "Unsupported built-in mesh primitive");
			result.AddDetail({ DiagnosticSeverity::Error, "asset.mesh.primitive.invalid", "The requested built-in mesh primitive is not supported", std::string(ToString(primitive)) });
			return result;
		}

		std::error_code errorCode;
		const auto parentPath = normalizedPath.AbsolutePath.parent_path();
		if (!parentPath.empty()) {
			std::filesystem::create_directories(parentPath, errorCode);
			if (errorCode) {
				auto result = ResultEnvelope::Failure("asset.create_builtin_mesh", normalizedPath.AssetId, "Failed to create mesh asset directory");
				result.AddDetail({ DiagnosticSeverity::Error, "asset.directory.create_failed", errorCode.message(), parentPath.generic_string() });
				return result;
			}
		}

		if (!Rendering::Mesh::SaveToFile(*mesh, normalizedPath.AbsolutePath.string())) {
			auto result = ResultEnvelope::Failure("asset.create_builtin_mesh", normalizedPath.AssetId, "Failed to persist built-in mesh asset");
			result.AddDetail({ DiagnosticSeverity::Error, "asset.mesh.save_failed", "Mesh::SaveToFile returned false", normalizedPath.AbsolutePath.generic_string() });
			return result;
		}

		auto result = RegisterMeshAsset(context, normalizedPath.AssetId, mesh, outHandle);
		result.Operation = "asset.create_builtin_mesh";
		result.Summary = "Built-in mesh asset created";
		result.SetPayloadValue("primitive", std::string(ToString(primitive)));
		result.SetPayloadValue("mesh_name", mesh->GetName());
		return result;
	}

	ResultEnvelope AssetService::RegisterMeshAsset(
		const ProjectContext& context,
		std::string_view assetId,
		const Ref<Rendering::Mesh>& mesh,
		AssetHandle* outHandle) {
		if (!mesh) {
			auto result = ResultEnvelope::Failure("asset.register_mesh", std::string(assetId), "Mesh asset is null");
			result.AddDetail({ DiagnosticSeverity::Error, "asset.mesh.null", "Mesh registration requires a valid mesh instance", {} });
			return result;
		}

		NormalizedAssetPath normalizedPath;
		ResultEnvelope normalizeError;
		if (!TryNormalizeAssetPath(context, assetId, normalizedPath, normalizeError)) {
			normalizeError.Operation = "asset.register_mesh";
			return normalizeError;
		}

		if (m_Manifest.Empty()) {
			auto manifestResult = LoadOrCreateManifest(context);
			if (!manifestResult.Succeeded()) {
				manifestResult.Operation = "asset.register_mesh";
				return manifestResult;
			}
		}

		AssetManifestRecord manifestRecord;
		manifestRecord.Guid = GetExistingGuidOrGenerate(m_Registry, m_Manifest, normalizedPath.AssetId);
		manifestRecord.Kind = AssetKind::Mesh;
		manifestRecord.Source = AssetSource::File;
		manifestRecord.AssetId = normalizedPath.AssetId;
		manifestRecord.RelativePath = normalizedPath.RelativePath;
		manifestRecord.ImportState = AssetImportState::Registered;
		if (!m_Manifest.Upsert(manifestRecord)) {
			return ResultEnvelope::Failure("asset.register_mesh", normalizedPath.AssetId, "Mesh manifest record conflicts with an existing asset");
		}
		auto saveResult = SaveAssetManifest(context, m_Manifest);
		if (!saveResult.Succeeded()) {
			saveResult.Operation = "asset.register_mesh";
			return saveResult;
		}

		auto record = MakeRegistryRecord(context, manifestRecord);
		record.AbsolutePath = normalizedPath.AbsolutePath;
		record.ExistsOnDisk = normalizedPath.ExistsOnDisk;
		const auto handle = m_Registry.Upsert(record);
		if (handle == 0) {
			return ResultEnvelope::Failure("asset.register_mesh", normalizedPath.AssetId, "Mesh registry record conflicts with an existing asset");
		}
		m_RuntimeCache.StoreMesh(manifestRecord.Guid, mesh);
		Rendering::MeshManager::Instance().RegisterMesh(normalizedPath.AssetId, mesh);
		if (outHandle) {
			*outHandle = handle;
		}

		return MakeRegistrationResult("asset.register_mesh", *m_Registry.Find(handle), "Mesh asset registered");
	}

	ResultEnvelope AssetService::LoadMeshAsset(
		const ProjectContext& context,
		std::string_view assetId,
		AssetHandle* outHandle) {
		NormalizedAssetPath normalizedPath;
		ResultEnvelope normalizeError;
		if (!TryNormalizeAssetPath(context, assetId, normalizedPath, normalizeError)) {
			normalizeError.Operation = "asset.load_mesh";
			return normalizeError;
		}

		std::error_code errorCode;
		if (!std::filesystem::is_regular_file(normalizedPath.AbsolutePath, errorCode)) {
			auto result = ResultEnvelope::Failure("asset.load_mesh", normalizedPath.AssetId, "Mesh asset file does not exist");
			result.AddDetail({ DiagnosticSeverity::Error, "asset.mesh.file_missing", "Mesh asset file must exist before loading", normalizedPath.AbsolutePath.generic_string() });
			return result;
		}

		auto mesh = Rendering::Mesh::LoadFromFile(normalizedPath.AbsolutePath.generic_string());
		if (!mesh) {
			auto result = ResultEnvelope::ManualIntervention("asset.load_mesh", normalizedPath.AssetId, "Mesh asset file exists but could not be deserialized");
			result.AddDetail({ DiagnosticSeverity::Error, "asset.mesh.deserialize_failed", "Mesh::LoadFromFile returned null", normalizedPath.AbsolutePath.generic_string() });
			return result;
		}

		return RegisterMeshAsset(context, normalizedPath.AssetId, mesh, outHandle);
	}

	ResultEnvelope AssetService::RegisterMaterialAsset(
		const ProjectContext& context,
		std::string_view assetId,
		const Ref<Rendering::Material>& material,
		AssetHandle* outHandle) {
		if (!material) {
			auto result = ResultEnvelope::Failure("asset.register_material", std::string(assetId), "Material asset is null");
			result.AddDetail({ DiagnosticSeverity::Error, "asset.material.null", "Material registration requires a valid material instance", {} });
			return result;
		}

		NormalizedAssetPath normalizedPath;
		ResultEnvelope normalizeError;
		if (!TryNormalizeAssetPath(context, assetId, normalizedPath, normalizeError)) {
			normalizeError.Operation = "asset.register_material";
			return normalizeError;
		}

		if (m_Manifest.Empty()) {
			auto manifestResult = LoadOrCreateManifest(context);
			if (!manifestResult.Succeeded()) {
				manifestResult.Operation = "asset.register_material";
				return manifestResult;
			}
		}

		AssetManifestRecord manifestRecord;
		manifestRecord.Guid = GetExistingGuidOrGenerate(m_Registry, m_Manifest, normalizedPath.AssetId);
		manifestRecord.Kind = AssetKind::Material;
		manifestRecord.Source = AssetSource::File;
		manifestRecord.AssetId = normalizedPath.AssetId;
		manifestRecord.RelativePath = normalizedPath.RelativePath;
		manifestRecord.ImportState = AssetImportState::Registered;
		if (!m_Manifest.Upsert(manifestRecord)) {
			return ResultEnvelope::Failure("asset.register_material", normalizedPath.AssetId, "Material manifest record conflicts with an existing asset");
		}
		auto saveResult = SaveAssetManifest(context, m_Manifest);
		if (!saveResult.Succeeded()) {
			saveResult.Operation = "asset.register_material";
			return saveResult;
		}

		auto record = MakeRegistryRecord(context, manifestRecord);
		record.AbsolutePath = normalizedPath.AbsolutePath;
		record.ExistsOnDisk = normalizedPath.ExistsOnDisk;
		const auto handle = m_Registry.Upsert(record);
		if (handle == 0) {
			return ResultEnvelope::Failure("asset.register_material", normalizedPath.AssetId, "Material registry record conflicts with an existing asset");
		}
		m_RuntimeCache.StoreMaterial(manifestRecord.Guid, material);
		Rendering::MaterialLibrary::Instance().RegisterMaterial(normalizedPath.AssetId, material);
		if (!material->GetName().empty() && material->GetName() != normalizedPath.AssetId) {
			Rendering::MaterialLibrary::Instance().RegisterMaterial(material->GetName(), material);
		}
		if (outHandle) {
			*outHandle = handle;
		}

		return MakeRegistrationResult("asset.register_material", *m_Registry.Find(handle), "Material asset registered");
	}

	ResultEnvelope AssetService::LoadMaterialAsset(
		const ProjectContext& context,
		std::string_view assetId,
		AssetHandle* outHandle) {
		NormalizedAssetPath normalizedPath;
		ResultEnvelope normalizeError;
		if (!TryNormalizeAssetPath(context, assetId, normalizedPath, normalizeError)) {
			normalizeError.Operation = "asset.load_material";
			return normalizeError;
		}

		std::error_code errorCode;
		if (!std::filesystem::is_regular_file(normalizedPath.AbsolutePath, errorCode)) {
			auto result = ResultEnvelope::Failure("asset.load_material", normalizedPath.AssetId, "Material asset file does not exist");
			result.AddDetail({ DiagnosticSeverity::Error, "asset.material.file_missing", "Material asset file must exist before loading", normalizedPath.AbsolutePath.generic_string() });
			return result;
		}

		auto material = Rendering::Material::CreateFromDeserialization();
		if (!material || !Serialization::LoadMaterial(normalizedPath.AbsolutePath.generic_string(), *material)) {
			auto result = ResultEnvelope::ManualIntervention("asset.load_material", normalizedPath.AssetId, "Material asset file exists but could not be deserialized");
			result.AddDetail({ DiagnosticSeverity::Error, "asset.material.deserialize_failed", "Material deserialization returned false", normalizedPath.AbsolutePath.generic_string() });
			return result;
		}

		return RegisterMaterialAsset(context, normalizedPath.AssetId, material, outHandle);
	}

	ResultEnvelope AssetService::RegisterTextureAsset(
		const ProjectContext& context,
		std::string_view assetId,
		const Ref<Rendering::Texture2D>& texture,
		AssetHandle* outHandle) {
		NormalizedAssetPath normalizedPath;
		ResultEnvelope normalizeError;
		if (!TryNormalizeAssetPath(context, assetId, normalizedPath, normalizeError)) {
			normalizeError.Operation = "asset.register_texture";
			return normalizeError;
		}

		if (m_Manifest.Empty()) {
			auto manifestResult = LoadOrCreateManifest(context);
			if (!manifestResult.Succeeded()) {
				manifestResult.Operation = "asset.register_texture";
				return manifestResult;
			}
		}

		AssetRecord record;
		record.Guid = GetExistingGuidOrGenerate(m_Registry, m_Manifest, normalizedPath.AssetId);
		record.Kind = AssetKind::Texture2D;
		record.Source = AssetSource::File;
		record.AssetId = normalizedPath.AssetId;
		record.RelativePath = normalizedPath.RelativePath;
		record.AbsolutePath = normalizedPath.AbsolutePath;
		std::error_code errorCode;
		record.ExistsOnDisk = std::filesystem::is_regular_file(normalizedPath.AbsolutePath, errorCode);
		record.ImportState = record.ExistsOnDisk ? AssetImportState::Registered : AssetImportState::Missing;

		if (!record.ExistsOnDisk) {
			auto result = ResultEnvelope::ManualIntervention("asset.register_texture", normalizedPath.AssetId, "Texture asset is unresolved");
			result.SetPayloadValue("asset_id", normalizedPath.AssetId);
			result.SetPayloadValue("asset_kind", std::string(ToString(AssetKind::Texture2D)));
			result.SetPayloadValue("asset_path", normalizedPath.AbsolutePath.generic_string());
			result.SetPayloadValue("exists_on_disk", "false");
			result.AddDetail({ DiagnosticSeverity::Warning, "asset.texture.unresolved", "Task 1 texture registration requires an existing source file; runtime-only texture payloads are not stored in metadata registry", normalizedPath.AbsolutePath.generic_string() });
			return result;
		}

		AssetManifestRecord manifestRecord;
		manifestRecord.Guid = record.Guid;
		manifestRecord.Kind = record.Kind;
		manifestRecord.Source = record.Source;
		manifestRecord.AssetId = record.AssetId;
		manifestRecord.RelativePath = record.RelativePath;
		manifestRecord.ImportState = record.ImportState;
		if (!m_Manifest.Upsert(manifestRecord)) {
			return ResultEnvelope::Failure("asset.register_texture", normalizedPath.AssetId, "Texture manifest record conflicts with an existing asset");
		}
		auto saveResult = SaveAssetManifest(context, m_Manifest);
		if (!saveResult.Succeeded()) {
			saveResult.Operation = "asset.register_texture";
			return saveResult;
		}

		const auto handle = m_Registry.Upsert(record);
		if (handle == 0) {
			return ResultEnvelope::Failure("asset.register_texture", normalizedPath.AssetId, "Texture registry record conflicts with an existing asset");
		}
		if (texture) {
			m_RuntimeCache.StoreTexture(record.Guid, texture);
		}
		if (outHandle) {
			*outHandle = handle;
		}

		return MakeRegistrationResult("asset.register_texture", *m_Registry.Find(handle), "Texture asset registered");
	}

	ResultEnvelope AssetService::ResolveAsset(AssetHandle handle, AssetRecord& outRecord) const {
		const auto* record = m_Registry.Find(handle);
		if (!record) {
			return MakeResolveFailure("asset.resolve", HandleToString(handle), "Asset handle was not found");
		}

		outRecord = *record;
		auto result = ResultEnvelope::Success("asset.resolve", HandleToString(handle), "Asset resolved by handle");
		result.SetPayloadValue("asset_handle", HandleToString(record->Handle));
		result.SetPayloadValue("asset_id", record->AssetId);
		result.SetPayloadValue("asset_kind", std::string(ToString(record->Kind)));
		return result;
	}

	ResultEnvelope AssetService::ResolveAsset(std::string_view assetId, AssetRecord& outRecord) const {
		const auto* record = m_Registry.Find(assetId);
		if (!record) {
			return MakeResolveFailure("asset.resolve", std::string(assetId), "Asset id was not found");
		}

		outRecord = *record;
		auto result = ResultEnvelope::Success("asset.resolve", record->AssetId, "Asset resolved by id");
		result.SetPayloadValue("asset_handle", HandleToString(record->Handle));
		result.SetPayloadValue("asset_kind", std::string(ToString(record->Kind)));
		return result;
	}

	ResultEnvelope AssetService::ResolveMeshAsset(AssetHandle handle, Ref<Rendering::Mesh>& outMesh) const {
		const auto* record = m_Registry.Find(handle);
		if (!record) {
			return MakeResolveFailure("asset.resolve_mesh", HandleToString(handle), "Mesh asset handle was not found");
		}

		if (record->Kind != AssetKind::Mesh) {
			auto result = ResultEnvelope::Failure("asset.resolve_mesh", HandleToString(handle), "Asset handle is not a mesh asset");
			result.AddDetail({ DiagnosticSeverity::Error, "asset.kind.mismatch", "Requested mesh resolve for a non-mesh asset", std::string(ToString(record->Kind)) });
			return result;
		}

		AssetResolver resolver(const_cast<AssetService&>(*this));
		auto result = resolver.ResolveMesh(record->Guid, outMesh);
		result.Target = HandleToString(handle);
		result.SetPayloadValue("asset_id", record->AssetId);
		return result;
	}

	ResultEnvelope AssetService::ResolveMaterialAsset(AssetHandle handle, Ref<Rendering::Material>& outMaterial) const {
		const auto* record = m_Registry.Find(handle);
		if (!record) {
			return MakeResolveFailure("asset.resolve_material", HandleToString(handle), "Material asset handle was not found");
		}

		if (record->Kind != AssetKind::Material) {
			auto result = ResultEnvelope::Failure("asset.resolve_material", HandleToString(handle), "Asset handle is not a material asset");
			result.AddDetail({ DiagnosticSeverity::Error, "asset.kind.mismatch", "Requested material resolve for a non-material asset", std::string(ToString(record->Kind)) });
			return result;
		}

		AssetResolver resolver(const_cast<AssetService&>(*this));
		auto result = resolver.ResolveMaterial(record->Guid, outMaterial);
		result.Target = HandleToString(handle);
		result.SetPayloadValue("asset_id", record->AssetId);
		return result;
	}

	ResultEnvelope AssetService::ResolveTextureAsset(AssetHandle handle, Ref<Rendering::Texture2D>& outTexture) const {
		const auto* record = m_Registry.Find(handle);
		if (!record) {
			return MakeResolveFailure("asset.resolve_texture", HandleToString(handle), "Texture asset handle was not found");
		}

		if (record->Kind != AssetKind::Texture2D) {
			auto result = ResultEnvelope::Failure("asset.resolve_texture", HandleToString(handle), "Asset handle is not a texture asset");
			result.AddDetail({ DiagnosticSeverity::Error, "asset.kind.mismatch", "Requested texture resolve for a non-texture asset", std::string(ToString(record->Kind)) });
			return result;
		}

		AssetResolver resolver(const_cast<AssetService&>(*this));
		auto result = resolver.ResolveTexture(record->Guid, outTexture);
		result.Target = HandleToString(handle);
		result.SetPayloadValue("asset_id", record->AssetId);
		return result;
	}

	const AssetRecord* AssetService::FindRecordByGuid(const AssetGuid& guid) const {
		return m_Registry.FindByGuid(guid);
	}

	ResultEnvelope AssetService::ValidateRegistry(const ProjectContext& context, AssetValidationReport* outReport) const {
		AssetValidationReport report;
		if (!context.IsLoaded()) {
			auto result = ResultEnvelope::Failure("asset.validate", {}, "Project context is not loaded");
			result.AddDetail({ DiagnosticSeverity::Error, "asset.project.unloaded", "Asset validation requires a loaded project context", {} });
			if (outReport) {
				*outReport = report;
			}
			return result;
		}

		std::error_code errorCode;
		const auto assetRoot = std::filesystem::absolute(context.GetAssetRootPath(), errorCode).lexically_normal();
		if (errorCode) {
			auto result = ResultEnvelope::Failure("asset.validate", context.GetTargetId(), "Failed to resolve project asset root");
			result.AddDetail({ DiagnosticSeverity::Error, "asset.root.resolve_failed", errorCode.message(), context.GetAssetRootPath().generic_string() });
			if (outReport) {
				*outReport = report;
			}
			return result;
		}

		m_Registry.ForEachRecord([&](const AssetRecord& record) {
			++report.TotalAssets;

			switch (record.Kind) {
			case AssetKind::Mesh:
				++report.MeshAssets;
				if (!Rendering::MeshManager::Instance().GetMesh(record.AssetId)) {
					++report.MeshAssetsMissingRuntimePayload;
				}
				break;
			case AssetKind::Material:
				++report.MaterialAssets;
				if (!Rendering::MaterialLibrary::Instance().GetMaterial(record.AssetId)) {
					++report.MaterialAssetsMissingRuntimePayload;
				}
				break;
			case AssetKind::Texture2D:
				++report.TextureAssets;
				if (record.ExistsOnDisk) {
					++report.SourceOnlyTextureAssets;
				}
				break;
			case AssetKind::Unknown:
			default:
				++report.UnknownKindAssets;
				break;
			}

			if (!record.IsOperational()) {
				++report.InvalidAssetRecords;
			}

			const auto resolvedAbsolutePath = record.AbsolutePath.empty()
				? (assetRoot / record.RelativePath).lexically_normal()
				: record.AbsolutePath.lexically_normal();
			if (IsOutsideAssetRoot(assetRoot, resolvedAbsolutePath)) {
				++report.AssetsOutsideProjectRoot;
			}
		});

		if (outReport) {
			*outReport = report;
		}

		auto result = report.IsOperational()
			? ResultEnvelope::Success("asset.validate", context.GetTargetId(), "Asset registry is operational")
			: ResultEnvelope::ManualIntervention("asset.validate", context.GetTargetId(), "Asset registry requires intervention");
		result.SetPayloadValue("asset_count", CountToString(report.TotalAssets));
		result.SetPayloadValue("mesh_asset_count", CountToString(report.MeshAssets));
		result.SetPayloadValue("material_asset_count", CountToString(report.MaterialAssets));
		result.SetPayloadValue("texture_asset_count", CountToString(report.TextureAssets));
		result.SetPayloadValue("unknown_kind_asset_count", CountToString(report.UnknownKindAssets));
		result.SetPayloadValue("invalid_asset_record_count", CountToString(report.InvalidAssetRecords));
		result.SetPayloadValue("assets_outside_project_root", CountToString(report.AssetsOutsideProjectRoot));
		result.SetPayloadValue("mesh_assets_missing_runtime_payload", CountToString(report.MeshAssetsMissingRuntimePayload));
		result.SetPayloadValue("material_assets_missing_runtime_payload", CountToString(report.MaterialAssetsMissingRuntimePayload));
		result.SetPayloadValue("source_only_texture_asset_count", CountToString(report.SourceOnlyTextureAssets));

		if (report.UnknownKindAssets > 0) {
			result.AddDetail({ DiagnosticSeverity::Error, "asset.kind.unknown", "One or more asset records have an unknown asset kind", CountToString(report.UnknownKindAssets) });
		}
		if (report.InvalidAssetRecords > 0) {
			result.AddDetail({ DiagnosticSeverity::Error, "asset.record.invalid", "One or more asset records are missing both disk and runtime backing", CountToString(report.InvalidAssetRecords) });
		}
		if (report.AssetsOutsideProjectRoot > 0) {
			result.AddDetail({ DiagnosticSeverity::Error, "asset.path.outside_root", "One or more asset records resolve outside the project asset root", CountToString(report.AssetsOutsideProjectRoot) });
		}
		if (report.MeshAssetsMissingRuntimePayload > 0) {
			result.AddDetail({ DiagnosticSeverity::Warning, "asset.mesh.unloaded", "One or more mesh asset records are missing runtime mesh payloads", CountToString(report.MeshAssetsMissingRuntimePayload) });
		}
		if (report.MaterialAssetsMissingRuntimePayload > 0) {
			result.AddDetail({ DiagnosticSeverity::Warning, "asset.material.unloaded", "One or more material asset records are missing runtime material payloads", CountToString(report.MaterialAssetsMissingRuntimePayload) });
		}
		if (report.SourceOnlyTextureAssets > 0) {
			result.AddDetail({ DiagnosticSeverity::Info, "asset.texture.source_only", "One or more texture assets currently exist as source-file-only records", CountToString(report.SourceOnlyTextureAssets) });
		}

		return result;
	}
}
