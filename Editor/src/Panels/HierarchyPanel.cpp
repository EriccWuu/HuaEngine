#include "enginepch.h"
#include "HierarchyPanel.h"

#include "entt.hpp"

#include "imgui.h"
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

#include "HuaEngine/ECS/Components.h"
#include "Interaction/EditorInteractionHost.h"
#include "Selection.h"

namespace HE {
    namespace {
        void DrawContextMenuEntries(EditorInteractionHost* host, std::string_view contextId) {
            if (!host) {
                return;
            }

            const auto* actions = host->ContextMenus().Find(contextId);
            if (!actions || actions->empty()) {
                ImGui::TextDisabled("No actions registered");
                return;
            }

            for (const auto& action : *actions) {
                const bool enabled = action.IsEnabled ? action.IsEnabled() : action.Enabled;
                if (ImGui::MenuItem(action.Label.c_str(), action.Shortcut.empty() ? nullptr : action.Shortcut.c_str(), false, enabled)) {
                    host->Commands().SetLastRoute(std::string("context.") + std::string(contextId) + "." + action.Id);
                    if (action.Trigger) {
                        action.Trigger();
                    }
                }

                if (!action.Tooltip.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                    ImGui::SetTooltip("%s", action.Tooltip.c_str());
                }
            }
        }
    }

	HierarchyPanel::HierarchyPanel(const Ref<Scene>& scene) {
		SetContext(scene);
	}

	void HierarchyPanel::OnGuiRender() {
		ImGui::Begin("Hierarchy");
        if (m_InteractionHost && m_InteractionHost->HasActiveScene()) {
            m_InteractionHost->Commands().SetLastRoute("panel.hierarchy");
        }
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

        HandleBackgroundSelectionClear();

        if (ImGui::BeginPopupContextWindow("HierarchyWindowContextMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
            DrawRegisteredContextMenu("HierarchyWindowContextMenu", "hierarchy.window");
            ImGui::EndPopup();
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
		if (Selection::IsSelected(entity)) {
			tree_flags |= ImGuiTreeNodeFlags_Selected;
		}
			
		bool node_open = ImGui::TreeNodeEx(std::to_string(entity.GetUid()).c_str(), tree_flags, "%s", entity.GetName().c_str());

		if (ImGui::IsItemClicked()) {
			HandleEntitySelection(entity);
		}

        if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && !Selection::IsSelected(entity)) {
            Selection::SetSelection(entity);
		}

        if (ImGui::BeginPopupContextItem("HierarchyEntityContextMenu")) {
            DrawRegisteredContextMenu("HierarchyEntityContextMenu", "hierarchy.entity");
            ImGui::EndPopup();
        }

        DrawDragDropSurface(entity);

		if (node_open) {
			ImGui::TreePop();
		}

		ImGui::PopID();
	}

    void HierarchyPanel::DrawRegisteredContextMenu(const char* popupId, std::string_view contextId) {
        (void)popupId;
        DrawContextMenuEntries(m_InteractionHost, contextId);
    }

    void HierarchyPanel::DrawDragDropSurface(Entity& entity) {
        if (!m_InteractionHost) {
            return;
        }

        const auto* intent = m_InteractionHost->DragDrop().Find("hierarchy.entity", "hierarchy.entity");
        if (!intent || !intent->Enabled) {
            return;
        }

        const uint32_t entityId = entity.GetUid();
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            ImGui::SetDragDropPayload(intent->PayloadType.c_str(), &entityId, sizeof(entityId));
            ImGui::TextUnformatted(intent->Label.empty() ? entity.GetName().c_str() : intent->Label.c_str());
            m_InteractionHost->Commands().SetLastRoute(std::string("drag_source.") + intent->Id);
            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(intent->PayloadType.c_str())) {
                (void)payload;
                m_InteractionHost->Commands().SetLastRoute(std::string("drag_target.") + intent->Id);
            }
            ImGui::EndDragDropTarget();
        }
    }

    void HierarchyPanel::HandleEntitySelection(const Entity& entity) {
        ImGuiIO& io = ImGui::GetIO();
        if (io.KeyCtrl) {
            Selection::ToggleSelection(entity);
            return;
        }

        Selection::SetSelection(entity);
    }

    void HierarchyPanel::HandleBackgroundSelectionClear() {
        if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup)) {
            return;
        }

        if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            return;
        }

        if (ImGui::IsAnyItemHovered()) {
            return;
        }

        Selection::ClearSelection();
    }

}
