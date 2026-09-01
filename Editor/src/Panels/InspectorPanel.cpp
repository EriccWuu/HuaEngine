#include "enginepch.h"
#include "Panels/InspectorPanel.h"

#include "Selection.h"
#include "imgui.h"

namespace HE {
	bool InspectorPanel::OnGuiRender() {
		bool changed = false;
		ImGui::Begin("Inspector");
		m_IsFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		m_IsHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
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
