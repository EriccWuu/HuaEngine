#include "enginepch.h"
#include "InspectorPanel.h"
#include "Selection.h"

namespace HE {
	void InspectorPanel::OnGuiRender() {
		ImGui::Begin("Inspector");
		if (Selection::HasSelection()) {
			auto& selection = Selection::GetSelection();
			ImGui::Text(selection.GetName().c_str());
			ComponentEditorRegistry::Instance().DrawComponents(selection.m_EntityManager->GetRegistry(), selection.m_EntityHandle);
		}
		ImGui::End();
	}
}