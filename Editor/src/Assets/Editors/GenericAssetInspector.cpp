#include "enginepch.h"
#include "Assets/Editors/GenericAssetInspector.h"

#include "imgui.h"

namespace HE::Editor {
	ResultEnvelope GenericAssetInspector::Open(const AssetEditorOpenContext& context) {
		m_Snapshot = context.Snapshot;
		return ResultEnvelope::Success("asset.editor.open", m_Snapshot.Asset.Guid, "Generic asset inspector opened");
	}

	void GenericAssetInspector::Draw(AssetEditorDrawContext&) {
		ImGui::TextUnformatted(m_Snapshot.Asset.AssetId.c_str());
		ImGui::Separator();
		ImGui::Text("Kind: %u", static_cast<uint32_t>(m_Snapshot.Asset.Kind));
		ImGui::TextWrapped("GUID: %s", m_Snapshot.Asset.Guid.c_str());
		ImGui::TextWrapped("Path: %s", m_Snapshot.Asset.RelativePath.generic_string().c_str());
		ImGui::Text("Importer: %s", m_Snapshot.ImporterId.empty() ? "Unavailable" : m_Snapshot.ImporterId.c_str());
		ImGui::Text("Import Health: %u", static_cast<uint32_t>(m_Snapshot.Health.State));
	}

	ResultEnvelope GenericAssetInspector::Validate() const {
		return m_Snapshot.Asset.IsOperational()
			? ResultEnvelope::Success("asset.edit.validate", m_Snapshot.Asset.Guid, "Asset is valid for read-only inspection")
			: ResultEnvelope::Failure("asset.edit.validate", m_Snapshot.Asset.Guid, "Asset metadata is invalid");
	}

	AssetEditCommit GenericAssetInspector::BuildCommit() const {
		return { .Guid = m_Snapshot.Asset.Guid };
	}
}
