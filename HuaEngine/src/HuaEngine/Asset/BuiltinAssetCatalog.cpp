#include "enginepch.h"
#include "BuiltinAssetCatalog.h"

#include "AssetSourcePath.h"
#include "HuaEngine/Core/ResourcePaths.h"

namespace HE {
	std::filesystem::path GetBuiltinAssetRootPath() {
		return ResourcePaths::GetEngineResourceRoot() / "BuiltinAssets";
	}

	ResultEnvelope LoadBuiltinAssetCatalog(
		const std::filesystem::path& builtinRoot,
		AssetManifest& outCatalog) {
		outCatalog = {};
		const auto catalogPath = builtinRoot / "manifest.json";
		auto result = LoadAssetManifest(catalogPath, outCatalog);
		result.Operation = "asset.builtin_catalog.load";
		if (!result.Succeeded()) {
			return result;
		}

		bool valid = true;
		outCatalog.ForEachRecord([&](const AssetManifestRecord& record) {
			if (record.Source != AssetSource::Builtin ||
				record.ImportState != AssetImportState::Builtin ||
				!IsSafeAssetRelativePath(record.RelativePath) ||
				record.AssetId.rfind("builtin/", 0) != 0) {
				valid = false;
			}
		});
		if (!valid) {
			auto failure = ResultEnvelope::Failure(
				"asset.builtin_catalog.load",
				catalogPath.generic_string(),
				"Builtin asset catalog contains an invalid record");
			failure.AddDetail({
				DiagnosticSeverity::Error,
				"asset.builtin_catalog.record_invalid",
				"Builtin records require source=builtin, import_state=builtin, a safe relative path, and a builtin asset id",
				catalogPath.generic_string()
			});
			outCatalog = {};
			return failure;
		}

		result.Summary = "Builtin asset catalog loaded";
		return result;
	}

	ResultEnvelope LoadBuiltinAssetCatalog(AssetManifest& outCatalog) {
		return LoadBuiltinAssetCatalog(GetBuiltinAssetRootPath(), outCatalog);
	}

	ResultEnvelope MergeBuiltinAssetCatalog(
		const AssetManifest& catalog,
		AssetManifest& inOutManifest) {
		auto result = ResultEnvelope::Success(
			"asset.builtin_catalog.merge",
			"builtin",
			"Builtin asset catalog merged");
		bool conflict = false;
		inOutManifest.ForEachRecord([&](const AssetManifestRecord& record) {
			if (record.Source != AssetSource::Builtin) {
				return;
			}

			const auto* catalogRecord = catalog.FindByGuid(record.Guid);
			if (!catalogRecord || catalogRecord->AssetId != record.AssetId) {
				conflict = true;
				result.AddDetail({
					DiagnosticSeverity::Error,
					"asset.builtin_catalog.unregistered",
					"Project metadata contains a builtin record that is not declared by the engine catalog",
					record.AssetId
				});
			}
		});
		if (conflict) {
			result.Status = OperationStatus::Failure;
			result.Summary = "Project metadata contains unregistered builtin assets";
			return result;
		}

		catalog.ForEachRecord([&](const AssetManifestRecord& record) {
			const auto* guidRecord = inOutManifest.FindByGuid(record.Guid);
			const auto* assetIdRecord = inOutManifest.FindByAssetId(record.AssetId);
			const bool guidConflict = guidRecord &&
				(guidRecord->AssetId != record.AssetId || guidRecord->Source != AssetSource::Builtin);
			const bool assetIdConflict = assetIdRecord &&
				(assetIdRecord->Guid != record.Guid || assetIdRecord->Source != AssetSource::Builtin);
			if (guidConflict || assetIdConflict) {
				conflict = true;
				result.AddDetail({
					DiagnosticSeverity::Error,
					"asset.builtin_catalog.conflict",
					"Project asset metadata conflicts with a reserved builtin GUID or asset id",
					record.AssetId
				});
			}
		});

		if (conflict) {
			result.Status = OperationStatus::Failure;
			result.Summary = "Builtin asset catalog conflicts with project metadata";
			return result;
		}

		catalog.ForEachRecord([&](const AssetManifestRecord& record) {
			(void)inOutManifest.Upsert(record);
		});
		return result;
	}
}
