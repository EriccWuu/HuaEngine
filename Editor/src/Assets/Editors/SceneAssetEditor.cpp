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
		if (m_Snapshot.SceneStatistics) {
			ImGui::Text("Name: %s", m_Snapshot.SceneStatistics->Name.c_str());
			ImGui::Text("Format version: %u", m_Snapshot.SceneStatistics->FormatVersion);
			ImGui::Text("Entities: %u", m_Snapshot.SceneStatistics->EntityCount);
		}
		const bool isActive = !context.ActiveScenePath.empty() &&
			context.ActiveScenePath.lexically_normal() == m_Snapshot.Asset.AbsolutePath.lexically_normal();
		ImGui::Text("Active scene: %s", isActive ? "yes" : "no");
		if (isActive) ImGui::Text("Active scene dirty: %s", context.ActiveSceneDirty ? "yes" : "no");
		ImGui::TextDisabled("Scene content is edited through the active scene document.");
		if (ImGui::Button("Open Scene") && context.OpenScene) context.OpenScene(m_Snapshot.Asset.AbsolutePath);
	}
}
