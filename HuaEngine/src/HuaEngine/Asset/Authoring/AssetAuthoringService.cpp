#include "enginepch.h"
#include "AssetAuthoringService.h"

#include "HuaEngine/Asset/Import/AssetSourceHash.h"
#include "HuaEngine/Asset/Library/AssetArtifactIO.h"
#include "HuaEngine/Asset/Metadata/AssetMeta.h"
#include "HuaEngine/Core/Sha256.h"

namespace HE {
	ResultEnvelope AssetAuthoringService::Apply(
		const ProjectContext& context,
		const AssetEditCommit& commit,
		AssetApplyState& outState,
		AssetReimportReport* outReport) {
		outState = AssetApplyState::ValidationFailed;
		if (!m_Assets || !context.IsLoaded() || commit.Guid.empty() || commit.SerializedContent.empty()) {
			return ResultEnvelope::Failure("asset.edit.apply", commit.Guid, "Asset edit commit is incomplete");
		}
		const auto* record = m_Assets->FindRecordByGuid(commit.Guid);
		if (!record || record->Source != AssetSource::File || record->AbsolutePath.empty()) {
			return ResultEnvelope::Failure("asset.edit.apply", commit.Guid, "Asset edit target is not a file asset");
		}
		std::string sourceHash;
		std::string metaHash;
		if (!ComputeAssetSourceHash(record->AbsolutePath, sourceHash).Succeeded() ||
			!ComputeAssetSourceHash(GetAssetMetaPath(record->AbsolutePath), metaHash).Succeeded()) {
			return ResultEnvelope::Failure("asset.edit.apply", commit.Guid, "Asset edit baseline could not be verified");
		}
		if (sourceHash != commit.ExpectedSourceHash || metaHash != commit.ExpectedMetaHash) {
			outState = AssetApplyState::Conflict;
			auto result = ResultEnvelope::ManualIntervention("asset.edit.apply", commit.Guid, "Asset source or metadata changed outside the edit session");
			result.AddDetail({ DiagnosticSeverity::Warning, "asset.edit.external_conflict", "Reload before applying changes", record->AbsolutePath.generic_string() });
			return result;
		}

		const auto contentHash = Sha256ToHex(ComputeSha256(commit.SerializedContent));
		const auto targetPath = commit.Target == AssetEditTarget::Source ? record->AbsolutePath : GetAssetMetaPath(record->AbsolutePath);
		if ((commit.Target == AssetEditTarget::Source ? sourceHash : metaHash) == contentHash) {
			outState = AssetApplyState::NoChanges;
			return ResultEnvelope::Success("asset.edit.apply", commit.Guid, "Asset edit has no changes");
		}
		auto saveResult = WriteAssetBinaryFileAtomically(targetPath, commit.SerializedContent, "asset.edit.save");
		if (!saveResult.Succeeded()) {
			outState = AssetApplyState::SaveFailed;
			saveResult.AddDetail({ DiagnosticSeverity::Error, "asset.edit.save_failed", "Authoring data could not be saved", targetPath.generic_string() });
			return saveResult;
		}

		AssetReimportReport report;
		auto importResult = m_Assets->ReimportAssets(context, record->AbsolutePath, &report);
		if (outReport) *outReport = report;
		if (!importResult.Succeeded() || report.FailedAssets > 0) {
			outState = AssetApplyState::SavedButImportFailed;
			auto result = ResultEnvelope::ManualIntervention("asset.edit.apply", commit.Guid, "Authoring data was saved but import failed");
			result.AddDetail({ DiagnosticSeverity::Error, "asset.edit.saved_import_failed", "The last-good artifact remains active", targetPath.generic_string() });
			for (auto& detail : importResult.Details) result.AddDetail(std::move(detail));
			return result;
		}
		outState = AssetApplyState::Applied;
		return ResultEnvelope::Success("asset.edit.apply", commit.Guid, "Asset authoring data saved and imported");
	}
}
