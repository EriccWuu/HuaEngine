#include "enginepch.h"
#include "InspectorPanel.h"

#include <typeindex>

#include "Interaction/EditorInteractionHost.h"
#include "Selection.h"

namespace HE {
    namespace {
        const EditorInspectableComponentDescriptor* FindInspectableComponentByType(std::type_index type) {
            if (type == std::type_index(typeid(Rendering::CameraComponent))) {
                return FindEditorInspectableComponent("component.camera");
            }
            if (type == std::type_index(typeid(Rendering::MeshComponent))) {
                return FindEditorInspectableComponent("component.mesh");
            }
            if (type == std::type_index(typeid(Rendering::MaterialComponent))) {
                return FindEditorInspectableComponent("component.material");
            }

            return nullptr;
        }

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

	bool InspectorPanel::OnGuiRender() {
		bool changed = false;
		ImGui::Begin("Inspector");
        if (m_InteractionHost && m_InteractionHost->HasActiveScene()) {
            m_InteractionHost->Commands().SetLastRoute("panel.inspector");
        }
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
            if (!Selection::HasSingleSelection()) {
                const auto& selections = Selection::GetSelections();
                ImGui::Text("%zu entities selected", selections.size());
                ImGui::TextDisabled("Multi-selection is currently summary-only.");
                ImGui::Separator();
                const size_t previewCount = (std::min)(selections.size(), size_t(6));
                for (size_t index = 0; index < previewCount; ++index) {
                    ImGui::BulletText("%s", selections[index].GetName().c_str());
                }
                if (selections.size() > previewCount) {
                    ImGui::TextDisabled("...and %zu more", selections.size() - previewCount);
                }

                if (ImGui::BeginPopupContextWindow("InspectorEntityContextMenu")) {
                    ImGui::TextDisabled("Multi-selection is summary-only.");
                    ImGui::EndPopup();
                }

                DrawAddComponentWindow();
                ImGui::End();
                return changed;
            }

			auto& selection = Selection::GetPrimarySelection();
			ImGui::Text("%s", selection.GetName().c_str());
            if (ImGui::BeginPopupContextWindow("InspectorEntityContextMenu")) {
                if (ImGui::MenuItem("Add Component...")) {
                    RequestOpenAddComponentWindow();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
			changed |= ComponentEditorRegistry::Instance().DrawComponents(
                selection.m_EntityManager->GetRegistry(),
                selection.m_EntityHandle,
                {
                    .RequestRemove = [this](std::type_index type) {
                        if (const auto* descriptor = FindInspectableComponentByType(type)) {
                            if (m_RemoveComponentCallback) {
                                m_RemoveComponentCallback(descriptor->Type);
                            }
                        }
                    },
                    .CanRemove = [](std::type_index type) {
                        if (!Selection::HasSingleSelection()) {
                            return false;
                        }

                        const auto* descriptor = FindInspectableComponentByType(type);
                        return descriptor != nullptr && CanRemoveInspectableComponent(descriptor->Type, Selection::GetPrimarySelection());
                    }
                });
            DrawAddComponentWindow();
		}
		else {
			ImGui::TextUnformatted("No entity selected.");
            if (ImGui::BeginPopupContextWindow("InspectorWindowContextMenu")) {
                ImGui::TextDisabled("Select an entity first.");
                ImGui::EndPopup();
            }
		}
		ImGui::End();
		return changed;
	}

    bool InspectorPanel::DrawRegisteredContextMenu(std::string_view contextId) {
        if (!m_InteractionHost) {
            return false;
        }

        const auto* actions = m_InteractionHost->ContextMenus().Find(contextId);
        if (!actions || actions->empty()) {
            return false;
        }

        DrawContextMenuEntries(m_InteractionHost, contextId);
        return true;
    }

    void InspectorPanel::DrawAddComponentWindow() {
        if (!m_ShowAddComponentWindow) {
            return;
        }

        ImGui::SetNextWindowSize(ImVec2(320.0f, 240.0f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Add Component", &m_ShowAddComponentWindow, ImGuiWindowFlags_NoCollapse)) {
            ImGui::End();
            return;
        }

        if (!Selection::HasSingleSelection()) {
            ImGui::TextDisabled("Select a single entity to add components.");
            ImGui::End();
            return;
        }

        const auto& selection = Selection::GetPrimarySelection();
        for (const auto& descriptor : GetEditorInspectableComponents()) {
            const bool alreadyHas = EntityHasInspectableComponent(descriptor.Type, selection);
            if (ImGui::Selectable(descriptor.DisplayName.c_str(), false, 0, ImVec2(0.0f, 0.0f)) && !alreadyHas) {
                if (m_AddComponentCallback) {
                    m_AddComponentCallback(descriptor.Type);
                }
                m_ShowAddComponentWindow = false;
            }

            if (alreadyHas) {
                ImGui::SameLine();
                ImGui::TextDisabled("(Already Added)");
            }

            if (alreadyHas && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("%s already exists on the selected entity.", descriptor.DisplayName.c_str());
            }
        }

        ImGui::End();
    }
}
