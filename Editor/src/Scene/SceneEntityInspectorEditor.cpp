#include "enginepch.h"
#include "Scene/SceneEntityInspectorEditor.h"

#include <algorithm>
#include <string>
#include <string_view>

#include "HuaEngine/Application.h"
#include "HuaEngine/Application/ApplicationOperations.h"
#include "Interaction/EditorInteractionHost.h"
#include "Selection.h"
#include "Workbench/EditorWorkbenchState.h"
#include "Workbench/SceneDocument.h"
#include "imgui.h"

namespace HE::Editor {
	namespace {
		const EditorInspectableComponentDescriptor* FindInspectableComponentByRuntimeType(std::string_view qualifiedName) {
			if (qualifiedName == "HE::Rendering::CameraComponent") return FindEditorInspectableComponent("component.camera");
			if (qualifiedName == "HE::Rendering::MeshComponent") return FindEditorInspectableComponent("component.mesh");
			if (qualifiedName == "HE::Rendering::MaterialComponent") return FindEditorInspectableComponent("component.material");
			return nullptr;
		}

		std::string GetComponentHeaderLabel(const ComponentMetadata& metadata) {
			if (metadata.RuntimeType) {
				const std::string displayName = GetRuntimeComponentDisplayName(*metadata.RuntimeType);
				if (!displayName.empty()) return displayName;
			}
			return !metadata.DisplayName.empty() ? metadata.DisplayName : metadata.TypeName;
		}
	}

	SceneEntityInspectorEditor::SceneEntityInspectorEditor(const AssetPickerCatalog& pickerCatalog)
		: m_PickerCatalog(pickerCatalog) {
		RegisterCoreComponents(m_ComponentRegistry);
	}

	bool SceneEntityInspectorEditor::HasEditingContext() const {
		return m_InteractionHost && m_InteractionHost->GetSceneDocument() &&
			m_InteractionHost->GetSceneDocument()->SceneRef;
	}

	bool SceneEntityInspectorEditor::Draw() {
		if (m_InteractionHost && m_InteractionHost->HasActiveScene()) {
			m_InteractionHost->Commands().SetLastRoute("panel.inspector");
		}

		if (!HasEditingContext()) {
			ImGui::TextUnformatted("No entity selected.");
			return false;
		}

		auto& world = m_InteractionHost->GetSceneDocument()->SceneRef->GetWorld();
		if (!Selection::HasSingleSelection()) {
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
			return false;
		}

		auto selection = Selection::ResolvePrimarySelection(world);
		if (!selection.IsValid()) {
			Selection::ClearSelection();
			ImGui::TextUnformatted("No entity selected.");
			return false;
		}

		bool changed = false;
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
			if (!metadata) {
				const std::string fallbackLabel = "Unknown Component " + std::to_string(typeId);
				if (ImGui::TreeNodeEx(fallbackLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
					ImGui::TextDisabled("Runtime metadata is not registered for this component.");
					ImGui::TreePop();
				}
				continue;
			}

			void* component = selection.TryGetComponentByType(typeId);
			if (!component) continue;

			const std::string headerLabel = GetComponentHeaderLabel(*metadata);
			ImGui::PushID(static_cast<int>(typeId));
			const bool open = ImGui::TreeNodeEx(headerLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
			if (metadata->RuntimeType && ImGui::BeginPopupContextItem("ComponentContextMenu")) {
				const auto* descriptor = FindInspectableComponentByRuntimeType(metadata->RuntimeType->QualifiedName);
				const bool canRemove = descriptor && CanRemoveInspectableComponent(descriptor->Type, selection);
				if (ImGui::MenuItem("Remove Component", nullptr, false, canRemove) && m_RemoveComponentCallback) {
					m_RemoveComponentCallback(descriptor->Type);
				}
				if (!canRemove && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
					ImGui::SetTooltip("Runtime remove command is not available for this component.");
				}
				ImGui::EndPopup();
			}

			if (open) {
				if (!metadata->RuntimeType) {
					ImGui::TextDisabled("Runtime descriptor is not available.");
				}
				else {
					changed |= DrawRuntimeComponentInspector(
						*metadata->RuntimeType,
						component,
						m_RuntimeOverrides,
						{
							.MeshAssets = m_PickerCatalog.Get(AssetKind::Mesh),
							.MaterialAssets = m_PickerCatalog.Get(AssetKind::Material),
							.TextureAssets = m_PickerCatalog.Get(AssetKind::Texture2D),
							.ResolveMaterialDefinition = [](const AssetGuid& guid, Rendering::MaterialDefinition& definition, AssetImportHealth& health) {
								return Application::GetInstance().GetOperations().GetMaterialDefinition(guid, definition, &health);
							},
							.CommitMaterialOverrides = [this, selection](const Rendering::MaterialOverrideSet& overrides) {
								if (!m_InteractionHost || !selection.IsValid() || !selection.HasComponent<Rendering::MaterialComponent>()) return;
								const auto before = selection.GetComponent<Rendering::MaterialComponent>().Overrides;
								(void)m_InteractionHost->ExecuteCommand(CreateSetMaterialOverridesCommand(selection, before, overrides));
							},
							.CommitMaterialReference = [this, selection](const AssetGuid& guid) {
								if (!m_InteractionHost || !selection.IsValid() || !selection.HasComponent<Rendering::MaterialComponent>()) return;
								const auto before = selection.GetComponent<Rendering::MaterialComponent>();
								auto after = before;
								after.Material.Reference.Guid = guid;
								Rendering::MaterialDefinition definition;
								const bool overridesRemoved = Application::GetInstance().GetOperations().GetMaterialDefinition(guid, definition).Succeeded() &&
									Rendering::ReconcileMaterialOverrides(after.Overrides, definition);
								const auto commandResult = m_InteractionHost->ExecuteCommand(CreateSetMaterialComponentCommand(selection, before, after));
								if (commandResult.Succeeded() && overridesRemoved && m_WorkbenchState) {
									auto result = ResultEnvelope::Success("editor.material_overrides.reconciled", guid, "Incompatible material overrides were removed");
									result.AddDetail({
										DiagnosticSeverity::Warning,
										"editor.material_overrides.removed",
										"The selected material does not define one or more previous overrides",
										guid
									});
									m_WorkbenchState->RecordEvent(result, "Inspector");
								}
							}
						});
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}

		DrawAddComponentWindow();
		ImGui::PopID();
		return changed;
	}

	void SceneEntityInspectorEditor::DrawAddComponentWindow() {
		if (!m_ShowAddComponentWindow) return;

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
		if (!HasEditingContext()) {
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
			const auto* descriptor = metadata.RuntimeType
				? FindInspectableComponentByRuntimeType(metadata.RuntimeType->QualifiedName)
				: nullptr;
			const bool canAdd = !alreadyHas && descriptor && m_AddComponentCallback;
			const ImGuiSelectableFlags flags = canAdd ? 0 : ImGuiSelectableFlags_Disabled;

			if (ImGui::Selectable(displayName.c_str(), false, flags, ImVec2(0.0f, 0.0f)) && canAdd) {
				m_AddComponentCallback(descriptor->Type);
				m_ShowAddComponentWindow = false;
			}
			if (alreadyHas) {
				ImGui::SameLine();
				ImGui::TextDisabled("(Already Added)");
			}
			else if (!descriptor) {
				ImGui::SameLine();
				ImGui::TextDisabled("(Runtime command unavailable)");
			}
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
				if (alreadyHas) ImGui::SetTooltip("%s already exists on the selected entity.", displayName.c_str());
				else if (!descriptor) ImGui::SetTooltip("Runtime add command is not available for this component.");
			}
		}
		ImGui::End();
	}
}
