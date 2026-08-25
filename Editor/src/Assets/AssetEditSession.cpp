#include "enginepch.h"
#include "Assets/AssetEditSession.h"

#include "HuaEngine/Asset/Import/AssetSourceHash.h"
#include "HuaEngine/Asset/Metadata/AssetMeta.h"

namespace HE::Editor {
	void AssetEditSession::Open(AssetInspectionSnapshot snapshot) {
		m_Snapshot = std::move(snapshot);
		m_Open = true;
		m_Dirty = false;
		m_ExternallyModified = false;
	}

	void AssetEditSession::Close() {
		m_Snapshot = {};
		m_Open = false;
		m_Dirty = false;
		m_ExternallyModified = false;
	}

	ResultEnvelope AssetEditSession::CheckExternalModification() {
		if (!m_Open || m_Snapshot.Asset.Source != AssetSource::File) return ResultEnvelope::Failure("asset.edit.external_check", GetGuid(), "No file asset edit session is open");
		std::string sourceHash;
		std::string metaHash;
		auto sourceResult = ComputeAssetSourceHash(m_Snapshot.Asset.AbsolutePath, sourceHash);
		auto metaResult = ComputeAssetSourceHash(GetAssetMetaPath(m_Snapshot.Asset.AbsolutePath), metaHash);
		if (!sourceResult.Succeeded() || !metaResult.Succeeded()) return ResultEnvelope::Failure("asset.edit.external_check", GetGuid(), "Asset source or metadata could not be hashed");
		m_ExternallyModified = sourceHash != m_Snapshot.SourceContentHash || metaHash != m_Snapshot.MetaContentHash;
		if (m_ExternallyModified) {
			auto result = ResultEnvelope::ManualIntervention("asset.edit.external_check", GetGuid(), "Asset source or metadata changed outside the edit session");
			result.AddDetail({ DiagnosticSeverity::Warning, "asset.edit.external_conflict", "Reload before applying changes", m_Snapshot.Asset.AbsolutePath.generic_string() });
			return result;
		}
		return ResultEnvelope::Success("asset.edit.external_check", GetGuid(), "Asset edit session matches disk state");
	}
}
