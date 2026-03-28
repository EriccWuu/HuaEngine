#include "enginepch.h"
#include "HierarchyPanel.h"

#include "entt.hpp"

#include "imgui.h"
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

#include "HuaEngine/ECS/Components.h"
#include "Selection.h"

namespace HE {
	HierarchyPanel::HierarchyPanel(const Ref<Scene>& scene) {
		SetContext(scene);
	}

	void HierarchyPanel::OnGuiRender() {
		ImGui::Begin("Hierarchy");
		if (m_WorkbenchState) {
			if (const auto* session = m_WorkbenchState->GetProjectSessionSummary()) {
				ImGui::Text("Project: %s", session->ProjectName.c_str());
			}

			if (const auto* scene = m_WorkbenchState->GetSceneDocumentSummary()) {
				ImGui::SameLine();
				ImGui::TextDisabled("| Scene: %s%s", scene->DisplayName.c_str(), scene->Dirty ? "*" : "");
			}

			if (const auto* result = m_WorkbenchState->GetLastResult()) {
				ImGui::Separator();
				ImGui::Text("Last Op: %s", result->Operation.c_str());
				ImGui::SameLine();
				ImGui::TextDisabled("[%s]", ToString(result->Status).data());
			}

			if (const auto* report = m_WorkbenchState->GetLastValidationReport()) {
				ImGui::Text("Validation Domains: %u", report->DomainCount);
				ImGui::SameLine();
				ImGui::Text("Warnings: %u Errors: %u", report->WarningCount, report->ErrorCount);
			}
			ImGui::Separator();
		}

		if (!m_Context) {
			ImGui::TextUnformatted("No scene loaded.");
			ImGui::End();
			return;
		}

		auto& entityManager = m_Context->GetEntityManager();
		auto& reg = entityManager.GetRegistry();
		auto view = reg.view<TransformComponent>();

		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_F, ImGuiInputFlags_Tooltip);
		ImGui::PushItemFlag(ImGuiItemFlags_NoNavDefaultFocus, true);
		if (ImGui::InputTextWithHint("##Filter", "", m_Filter.InputBuf, IM_ARRAYSIZE(m_Filter.InputBuf), ImGuiInputTextFlags_EscapeClearsAll))
			m_Filter.Build();
		ImGui::PopItemFlag();

		for (auto entity : view) {
			Entity e(entity, &entityManager);
			DrawEntityNode(e);
		}

		ImGui::End();
	}

	void HierarchyPanel::SetContext(const Ref<Scene>& context) {
		m_Context = context;
	}

	void HierarchyPanel::DrawEntityNode(Entity& entity) {
		ImGui::PushID(entity.GetUid());
		ImGuiTreeNodeFlags tree_flags = ImGuiTreeNodeFlags_None;
		tree_flags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;    // Standard opening mode as we are likely to want to add selection afterwards
		tree_flags |= ImGuiTreeNodeFlags_NavLeftJumpsBackHere;   
		tree_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;// Left arrow support
		if (Selection::HasSelection() && Selection::GetSelection() == entity) {
			tree_flags |= ImGuiTreeNodeFlags_Selected;
		}
			
		bool node_open = ImGui::TreeNodeEx(std::to_string(entity.GetUid()).c_str(), tree_flags, "%s", entity.GetName().c_str());

		if (ImGui::IsItemClicked()) {
			Selection::SetSelection(entity);
		}

		if (node_open) {
			ImGui::TreePop();
		}

		ImGui::PopID();
	}

}
