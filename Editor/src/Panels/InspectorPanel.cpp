#include "enginepch.h"
#include "InspectorPanel.h"
#include "Selection.h"

namespace HE {
	bool InspectorPanel::OnGuiRender() {
		bool changed = false;
		ImGui::Begin("Inspector");
		if (m_WorkbenchState) {
			if (const auto* session = m_WorkbenchState->GetProjectSessionSummary()) {
				ImGui::Text("Project: %s", session->ProjectName.c_str());
				if (const auto* scene = m_WorkbenchState->GetSceneDocumentSummary()) {
					ImGui::SameLine();
					ImGui::TextDisabled("| Scene: %s%s", scene->DisplayName.c_str(), scene->Dirty ? "*" : "");
				}
				ImGui::Separator();
			}

			if (const auto* validation = m_WorkbenchState->GetLastValidationResult()) {
				ImGui::Text("Validation: %s", ToString(validation->Status).data());
				ImGui::TextWrapped("%s", validation->Summary.c_str());
				ImGui::Separator();
			}
		}
		if (Selection::HasSelection()) {
			auto& selection = Selection::GetSelection();
			ImGui::Text("%s", selection.GetName().c_str());
			changed |= ComponentEditorRegistry::Instance().DrawComponents(selection.m_EntityManager->GetRegistry(), selection.m_EntityHandle);
		}
		else {
			ImGui::TextUnformatted("No entity selected.");
		}
		ImGui::End();
		return changed;
	}
}
