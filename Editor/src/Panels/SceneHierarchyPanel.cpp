#include "enginepch.h"
#include "SceneHierarchyPanel.h"

#include "entt.hpp"

#include "imgui.h"
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

#include "HuaEngine/ECS/Components.h"
#include "Selection.h"

namespace HE {
	SceneHierarchyPanel::SceneHierarchyPanel(const Ref<Scene>& scene) {
		SetContext(scene);
	}

	void SceneHierarchyPanel::OnGuiRender() {
		auto& entityManager = m_Context->GetEntityManager();
		auto& reg = entityManager.GetRegistry();
		auto view = reg.view<TransformComponent>();
		
		ImGui::Begin("Scene Hierarchy");
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

	void SceneHierarchyPanel::SetContext(const Ref<Scene>& context) {
		m_Context = context;
	}

	void SceneHierarchyPanel::DrawEntityNode(Entity& entity) {
		ImGui::PushID(entity.GetUid());
		ImGuiTreeNodeFlags tree_flags = ImGuiTreeNodeFlags_None;
		tree_flags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;    // Standard opening mode as we are likely to want to add selection afterwards
		tree_flags |= ImGuiTreeNodeFlags_NavLeftJumpsBackHere;   
		tree_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;// Left arrow support
		if (Selection::HasSelection() && Selection::GetSelection() == entity) {
			tree_flags |= ImGuiTreeNodeFlags_Selected;
		}
			
		bool node_open = ImGui::TreeNodeEx(std::to_string(entity.GetUid()).c_str(), tree_flags, entity.GetName().c_str());

		if (ImGui::IsItemClicked()) {
			Selection::SetSelection(entity);
		}

		if (node_open) {
			ImGui::TreePop();
		}

		ImGui::PopID();
	}

}