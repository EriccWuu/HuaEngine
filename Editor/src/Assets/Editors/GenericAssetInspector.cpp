#include "enginepch.h"
#include "Assets/Editors/GenericAssetInspector.h"

namespace HE::Editor {
	ResultEnvelope GenericAssetInspector::Open(const AssetEditorOpenContext& context) {
		m_Snapshot = context.Snapshot;
		return ResultEnvelope::Success("asset.editor.open", m_Snapshot.Asset.Guid, "Generic asset inspector opened");
	}

	void GenericAssetInspector::Draw(AssetEditorDrawContext&) {}

	ResultEnvelope GenericAssetInspector::Validate() const {
		return m_Snapshot.Asset.IsOperational()
			? ResultEnvelope::Success("asset.edit.validate", m_Snapshot.Asset.Guid, "Asset is valid for read-only inspection")
			: ResultEnvelope::Failure("asset.edit.validate", m_Snapshot.Asset.Guid, "Asset metadata is invalid");
	}

	AssetEditCommit GenericAssetInspector::BuildCommit() const {
		return { .Guid = m_Snapshot.Asset.Guid };
	}
}
