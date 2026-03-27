#include "enginepch.h"
#include "InspectorPanel.h"
#include "Selection.h"

namespace HE {
	void InspectorPanel::OnGuiRender() {
		ImGui::Begin("Inspector");
		if (m_WorkbenchState) {
			if (const auto* validation = m_WorkbenchState->GetLastValidationResult()) {
				ImGui::Text("Validation: %s", ToString(validation->Status).data());
				ImGui::TextWrapped("%s", validation->Summary.c_str());
				ImGui::Separator();
			}
		}
		if (Selection::HasSelection()) {
			auto& selection = Selection::GetSelection();
			ImGui::Text(selection.GetName().c_str());
			ComponentEditorRegistry::Instance().DrawComponents(selection.m_EntityManager->GetRegistry(), selection.m_EntityHandle);
		}
		else {
			ImGui::TextUnformatted("No entity selected.");
		}
		ImGui::End();
	}
}
