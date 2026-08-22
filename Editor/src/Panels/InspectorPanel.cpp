#include "enginepch.h"
#include "InspectorPanel.h"

#include <string>
#include <string_view>

#include "Interaction/EditorInteractionHost.h"
#include "Selection.h"
#include "Workbench/SceneDocument.h"

namespace HE {
    namespace {
        const EditorInspectableComponentDescriptor* FindInspectableComponentByRuntimeType(std::string_view qualifiedName) {
            if (qualifiedName == "HE::Rendering::CameraComponent") {
                return FindEditorInspectableComponent("component.camera");
            }
            if (qualifiedName == "HE::Rendering::MeshComponent") {
                return FindEditorInspectableComponent("component.mesh");
            }
            if (qualifiedName == "HE::Rendering::MaterialComponent") {
                return FindEditorInspectableComponent("component.material");
            }

            return nullptr;
        }

        std::string GetComponentHeaderLabel(const ComponentMetadata& metadata) {
            if (metadata.RuntimeType != nullptr) {
                const std::string displayName = Editor::GetRuntimeComponentDisplayName(*metadata.RuntimeType);
                if (!displayName.empty()) {
                    return displayName;
                }
            }
            if (!metadata.DisplayName.empty()) {
                return metadata.DisplayName;
            }
            return metadata.TypeName;
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

    InspectorPanel::InspectorPanel() {
        RegisterCoreComponents(m_ComponentRegistry);
    }

	void InspectorPanel::SetAssetRecords(std::span<const AssetRecord> records) {
		m_MeshAssetOptions = Editor::BuildAssetPickerOptions(records, AssetKind::Mesh);
		m_MaterialAssetOptions = Editor::BuildAssetPickerOptions(records, AssetKind::Material);
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
                if (!m_InteractionHost || !m_InteractionHost->GetSceneDocument() || !m_InteractionHost->GetSceneDocument()->SceneRef) {
                    ImGui::TextUnformatted("No entity selected.");
                    ImGui::End();
                    return changed;
                }

                auto& world = m_InteractionHost->GetSceneDocument()->SceneRef->GetWorld();
                const auto& selections = Selection::ResolveSelections(world);
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

            if (!m_InteractionHost || !m_InteractionHost->GetSceneDocument() || !m_InteractionHost->GetSceneDocument()->SceneRef) {
                ImGui::TextUnformatted("No entity selected.");
                ImGui::End();
                return changed;
            }

            auto& world = m_InteractionHost->GetSceneDocument()->SceneRef->GetWorld();
			auto selection = Selection::ResolvePrimarySelection(world);
            if (!selection.IsValid()) {
                Selection::ClearSelection();
                ImGui::TextUnformatted("No entity selected.");
                ImGui::End();
                return changed;
            }

			ImGui::PushID(static_cast<int>(selection.GetUid()));
			ImGui::Text("%s", selection.GetName().c_str());
            if (ImGui::BeginPopupContextWindow("InspectorEntityContextMenu")) {
                if (ImGui::MenuItem("Add Component...")) {
                    RequestOpenAddComponentWindow();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            for (const ComponentTypeId typeId : selection.ListComponentTypes()) {
                const ComponentMetadata* metadata = m_ComponentRegistry.FindByTypeId(typeId);
                if (metadata == nullptr) {
                    const std::string fallbackLabel = "Unknown Component " + std::to_string(typeId);
                    if (ImGui::TreeNodeEx(fallbackLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                        ImGui::TextDisabled("Runtime metadata is not registered for this component.");
                        ImGui::TreePop();
                    }
                    continue;
                }

                void* component = selection.TryGetComponentByType(typeId);
                if (component == nullptr) {
                    continue;
                }

                const std::string headerLabel = GetComponentHeaderLabel(*metadata);
                ImGui::PushID(static_cast<int>(typeId));
                const bool open = ImGui::TreeNodeEx(headerLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

                if (metadata->RuntimeType != nullptr) {
                    if (ImGui::BeginPopupContextItem("ComponentContextMenu")) {
                        const auto* descriptor = FindInspectableComponentByRuntimeType(metadata->RuntimeType->QualifiedName);
                        const bool canRemove = descriptor != nullptr && CanRemoveInspectableComponent(descriptor->Type, selection);
                        if (ImGui::MenuItem("Remove Component", nullptr, false, canRemove)) {
                            if (m_RemoveComponentCallback) {
                                m_RemoveComponentCallback(descriptor->Type);
                            }
                        }
                        if (!canRemove && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                            ImGui::SetTooltip("Runtime remove command is not available for this component.");
                        }
                        ImGui::EndPopup();
                    }
                }

                if (open) {
                    if (metadata->RuntimeType == nullptr) {
                        ImGui::TextDisabled("Runtime descriptor is not available.");
                    }
                    else {
						changed |= Editor::DrawRuntimeComponentInspector(
							*metadata->RuntimeType,
							component,
							m_RuntimeOverrides,
							{
								.MeshAssets = m_MeshAssetOptions,
								.MaterialAssets = m_MaterialAssetOptions
							});
                    }
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
            DrawAddComponentWindow();
			ImGui::PopID();
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

        if (!m_InteractionHost || !m_InteractionHost->GetSceneDocument() || !m_InteractionHost->GetSceneDocument()->SceneRef) {
            ImGui::End();
            return;
        }

        const auto selection = Selection::ResolvePrimarySelection(m_InteractionHost->GetSceneDocument()->SceneRef->GetWorld());
        if (!selection.IsValid()) {
            ImGui::End();
            return;
        }
        for (const ComponentMetadata& metadata : m_ComponentRegistry.GetAll()) {
            const bool alreadyHas = selection.TryGetComponentByType(metadata.TypeId) != nullptr;
            const std::string displayName = GetComponentHeaderLabel(metadata);
            const auto* descriptor = metadata.RuntimeType != nullptr
                ? FindInspectableComponentByRuntimeType(metadata.RuntimeType->QualifiedName)
                : nullptr;
            const bool canAdd = !alreadyHas && descriptor != nullptr && m_AddComponentCallback;
            const ImGuiSelectableFlags flags = canAdd ? 0 : ImGuiSelectableFlags_Disabled;

            if (ImGui::Selectable(displayName.c_str(), false, flags, ImVec2(0.0f, 0.0f)) && canAdd) {
                m_AddComponentCallback(descriptor->Type);
                m_ShowAddComponentWindow = false;
            }

            if (alreadyHas) {
                ImGui::SameLine();
                ImGui::TextDisabled("(Already Added)");
            }
            else if (descriptor == nullptr) {
                ImGui::SameLine();
                ImGui::TextDisabled("(Runtime command unavailable)");
            }

            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                if (alreadyHas) {
                    ImGui::SetTooltip("%s already exists on the selected entity.", displayName.c_str());
                }
                else if (descriptor == nullptr) {
                    ImGui::SetTooltip("Runtime add command is not available for this component.");
                }
            }
        }

        ImGui::End();
    }
}
