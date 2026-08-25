#include "enginepch.h"
#include "SceneAssetEditor.h"

#include "imgui.h"

namespace HE::Editor {
	ResultEnvelope SceneAssetEditor::Open(const AssetEditorOpenContext& context) {
		m_Snapshot = context.Snapshot;
		std::error_code errorCode;
		m_SourceBytes = std::filesystem::file_size(m_Snapshot.Asset.AbsolutePath, errorCode);
		if (errorCode) m_SourceBytes = 0;
		return ResultEnvelope::Success("asset.scene_editor.open", m_Snapshot.Asset.Guid, "Scene inspector opened");
	}

	ResultEnvelope SceneAssetEditor::Validate() const {
		return m_Snapshot.Asset.ExistsOnDisk
			? ResultEnvelope::Success("asset.scene_editor.validate", m_Snapshot.Asset.Guid, "Scene source is available")
			: ResultEnvelope::Failure("asset.scene_editor.validate", m_Snapshot.Asset.Guid, "Scene source is unavailable");
	}

	void SceneAssetEditor::Draw(AssetEditorDrawContext& context) {
		ImGui::Text("Source size: %llu bytes", static_cast<unsigned long long>(m_SourceBytes));
		ImGui::TextDisabled("Scene content is edited through the active scene document.");
		if (ImGui::Button("Open Scene") && context.OpenScene) context.OpenScene(m_Snapshot.Asset.AbsolutePath);
	}
}
