#include "enginepch.h"
#include "Panels/InspectorPanel.h"

#include "Selection.h"
#include "imgui.h"

namespace HE {
	bool InspectorPanel::OnGuiRender() {
		bool changed = false;
		ImGui::Begin("Inspector");
		if (Selection::HasAssetSelection()) {
			m_AssetEditor.Draw();
		}
		else if (Selection::HasSelection()) {
			changed = m_SceneEditor.Draw();
		}
		else {
			ImGui::TextDisabled("Nothing selected.");
		}
		ImGui::End();
		return changed;
	}
}
