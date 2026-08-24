#include "enginepch.h"
#include "AssetImportService.h"

#include <filesystem>
#include <algorithm>
#include <vector>

#include "HuaEngine/Asset/AssetSourcePath.h"
#include "HuaEngine/Asset/Import/AssetImportFingerprint.h"
#include "HuaEngine/Asset/Import/AssetSourceHash.h"

namespace HE {
	ResultEnvelope AssetImportService::ImportMissingAssets(
		const ProjectContext& context,
		const AssetManifest& manifest,
		AssetImportReport* outReport) const {
		std::vector<AssetGuid> assetGuids;
		manifest.ForEachRecord([&assetGuids](const AssetManifestRecord& record) {
			if (record.Source == AssetSource::File || record.Source == AssetSource::Builtin) {
				assetGuids.push_back(record.Guid);
			}
		});

		AssetImportReport report;
		auto result = ImportAssets(
			context,
			manifest,
			assetGuids,
			AssetImportPolicy::MissingOnly,
			&report);
		result.Operation = "asset.import_missing";
		if (result.Succeeded() && report.FailedAssets == 0) {
			result.Summary = "Missing asset artifacts imported";
		}
		if (outReport) {
			*outReport = std::move(report);
		}
		return result;
	}

	ResultEnvelope AssetImportService::ImportAssets(
		const ProjectContext& context,
		const AssetManifest& manifest,
		std::span<const AssetGuid> assetGuids,
		AssetImportPolicy policy,
		AssetImportReport* outReport) const {
		AssetImportReport report;
		auto result = ResultEnvelope::Success("asset.import_assets", context.GetTargetId(), "Asset artifacts imported");
		bool builtinFailure = false;

		std::vector<AssetGuid> orderedGuids(assetGuids.begin(), assetGuids.end());
		std::stable_sort(orderedGuids.begin(), orderedGuids.end(), [&](const auto& left, const auto& right) {
			const auto* leftRecord = manifest.FindByGuid(left);
			const auto* rightRecord = manifest.FindByGuid(right);
			const auto priority = [](AssetKind kind) { return kind == AssetKind::Shader || kind == AssetKind::Texture2D ? 0 : kind == AssetKind::Material ? 2 : 1; };
			return priority(leftRecord ? leftRecord->Kind : AssetKind::Unknown) < priority(rightRecord ? rightRecord->Kind : AssetKind::Unknown);
		});
		for (const auto& guid : orderedGuids) {
			const auto* record = manifest.FindByGuid(guid);
			if (!record || (record->Source != AssetSource::File && record->Source != AssetSource::Builtin)) {
				++report.FailedAssets;
				result.AddDetail({
					DiagnosticSeverity::Warning,
					"asset.import.record_missing",
					"Asset import requires a file-backed or builtin manifest record",
					guid
				});
				continue;
			}
			if (record->Source == AssetSource::Builtin) {
				++report.TotalBuiltinAssets;
			}
			else {
				++report.TotalFileAssets;
			}

			std::filesystem::path sourcePath;
			auto sourceResult = ResolveAssetSourcePath(context, *record, sourcePath);
			if (!sourceResult.Succeeded()) {
				++report.FailedAssets;
				builtinFailure |= record->Source == AssetSource::Builtin;
				for (auto& diagnostic : sourceResult.Details) {
					result.AddDetail(std::move(diagnostic));
				}
				continue;
			}
			const auto* importer = m_Registry->Find(record->Kind, sourcePath.extension().string());
			if (!importer) {
				++report.FailedAssets;
				builtinFailure |= record->Source == AssetSource::Builtin;
				result.AddDetail({
					DiagnosticSeverity::Warning,
					"asset.import.importer_missing",
					"No asset importer supports the manifest kind and source extension",
					record->AssetId
				});
				continue;
			}

			std::error_code errorCode;
			if (!std::filesystem::is_regular_file(sourcePath, errorCode)) {
				if (policy == AssetImportPolicy::MissingOnly && m_Library->IsArtifactAvailable(
					record->Guid,
					record->Kind,
					importer->GetId(),
					importer->GetVersion(),
					importer->GetArtifactVersion())) {
					++report.SkippedAssets;
					continue;
				}
				++report.FailedAssets;
				builtinFailure |= record->Source == AssetSource::Builtin;
				result.AddDetail({
					DiagnosticSeverity::Error,
					"asset.import.source_missing",
					"Asset source file is missing",
					sourcePath.generic_string()
				});
				continue;
			}

			std::string sourceContentHash;
			auto hashResult = ComputeAssetSourceHash(sourcePath, sourceContentHash);
			if (!hashResult.Succeeded()) {
				++report.FailedAssets;
				builtinFailure |= record->Source == AssetSource::Builtin;
				for (auto& diagnostic : hashResult.Details) {
					result.AddDetail(std::move(diagnostic));
				}
				continue;
			}
			const AssetImportContext importContext{
				.Project = context,
				.SourceAsset = *record,
				.SourcePath = sourcePath,
				.Manifest = &manifest,
				.Library = m_Library
			};
			AssetImportFingerprintInput fingerprintInput;
			auto fingerprintInputsResult = importer->BuildFingerprintInput(importContext, sourceContentHash, fingerprintInput);
			std::string importFingerprint;
			if (!fingerprintInputsResult.Succeeded() || !ComputeAssetImportFingerprint(fingerprintInput, importFingerprint).Succeeded()) {
				++report.FailedAssets;
				builtinFailure |= record->Source == AssetSource::Builtin;
				for (auto& diagnostic : fingerprintInputsResult.Details) result.AddDetail(std::move(diagnostic));
				continue;
			}

			if (policy == AssetImportPolicy::MissingOnly && m_Library->IsArtifactCurrent(
				record->Guid,
				record->Kind,
				importer->GetId(),
				importer->GetVersion(),
				importer->GetArtifactVersion(),
				importFingerprint)) {
				++report.SkippedAssets;
				continue;
			}

			auto importResult = importer->Import(importContext);
			if (!importResult.Success) {
				++report.FailedAssets;
				builtinFailure |= record->Source == AssetSource::Builtin;
				for (auto& diagnostic : importResult.Diagnostics) {
					result.AddDetail(std::move(diagnostic));
				}
				continue;
			}

			auto commitResult = m_Library->CommitArtifact(
				record->Guid,
				importer->GetId(),
				importer->GetVersion(),
				importFingerprint,
				importResult.Artifact);
			if (!commitResult.Succeeded()) {
				++report.FailedAssets;
				builtinFailure |= record->Source == AssetSource::Builtin;
				for (auto& diagnostic : commitResult.Details) {
					result.AddDetail(std::move(diagnostic));
				}
				continue;
			}

			++report.ImportedAssets;
			report.ImportedAssetGuids.push_back(record->Guid);
		}

		auto saveResult = m_Library->Save();
		if (!saveResult.Succeeded()) {
			if (outReport) {
				*outReport = report;
			}
			saveResult.Operation = "asset.import_assets";
			return saveResult;
		}

		result.SetPayloadValue("total_file_assets", std::to_string(report.TotalFileAssets));
		result.SetPayloadValue("total_builtin_assets", std::to_string(report.TotalBuiltinAssets));
		result.SetPayloadValue("imported_assets", std::to_string(report.ImportedAssets));
		result.SetPayloadValue("skipped_assets", std::to_string(report.SkippedAssets));
		result.SetPayloadValue("failed_assets", std::to_string(report.FailedAssets));
		if (report.FailedAssets > 0) {
			result.Summary = "Asset import completed with per-asset failures";
		}
		if (builtinFailure) {
			result.Status = OperationStatus::Failure;
			result.Summary = "Builtin asset import failed";
		}
		if (outReport) {
			*outReport = report;
		}
		return result;
	}
}
