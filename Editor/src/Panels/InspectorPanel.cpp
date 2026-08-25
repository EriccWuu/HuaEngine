#include "enginepch.h"
#include "InspectorPanel.h"

#include <string>
#include <string_view>

#include "Interaction/EditorInteractionHost.h"
#include "HuaEngine/Application.h"
#include "HuaEngine/Application/ApplicationOperations.h"
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
		Selection::GetService().SetChangeGuard([this](const Editor::EditorSelection& selection) {
			if (!HasDirtyAsset()) return true;
			if (const auto* asset = std::get_if<Editor::AssetSelection>(&selection);
				asset && asset->Guid == m_AssetInspectorHost.GetSession().GetGuid()) return true;
			RequestDirtyAssetResolution([selection]() mutable {
				Selection::GetService().AcceptGuardedSelection(std::move(selection));
			});
			return false;
		});
    }

	InspectorPanel::~InspectorPanel() {
		Selection::GetService().SetChangeGuard({});
	}

	bool InspectorPanel::HasDirtyAsset() const {
		const auto* editor = m_AssetInspectorHost.GetEditor();
		return editor != nullptr && editor->IsDirty();
	}

	ResultEnvelope InspectorPanel::ApplyAssetEdit() {
		auto* editor = m_AssetInspectorHost.GetEditor();
		const auto guid = m_AssetInspectorHost.GetSession().GetGuid();
		if (!editor || !m_ProjectContext || guid.empty()) {
			return ResultEnvelope::Failure("asset.editor.apply", guid, "No editable asset is active");
		}

		auto result = editor->Validate();
		if (result.Succeeded()) {
			AssetApplyState state;
			result = Application::GetInstance().GetOperations().ApplyAssetEdit(*m_ProjectContext, editor->BuildCommit(), state);
			if (state == AssetApplyState::Applied || state == AssetApplyState::SavedButImportFailed || state == AssetApplyState::NoChanges) {
				(void)m_AssetInspectorHost.Open(guid, [](const AssetGuid& assetGuid, AssetInspectionSnapshot& snapshot) {
					return Application::GetInstance().GetOperations().InspectAsset(assetGuid, snapshot);
				});
			}
		}
		if (m_WorkbenchState) m_WorkbenchState->RecordEvent(result, "Inspector");
		return result;
	}

	void InspectorPanel::RevertAssetEdit() {
		if (auto* editor = m_AssetInspectorHost.GetEditor()) editor->Revert();
	}

	bool InspectorPanel::RequestDirtyAssetResolution(std::function<void()> continuation) {
		if (!HasDirtyAsset()) return false;
		m_DirtyAssetContinuation = std::move(continuation);
		m_OpenDirtyAssetPopup = true;
		return true;
	}

	void InspectorPanel::OnDirtyAssetPopup() {
		if (m_OpenDirtyAssetPopup) {
			ImGui::OpenPopup("Unsaved Asset Changes");
			m_OpenDirtyAssetPopup = false;
		}

		if (!ImGui::BeginPopupModal("Unsaved Asset Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
		ImGui::TextWrapped("The current asset has unapplied changes. Apply before continuing?");
		ImGui::Spacing();

		if (ImGui::Button("Apply and Continue")) {
			if (ApplyAssetEdit().Succeeded()) {
				auto continuation = std::move(m_DirtyAssetContinuation);
				ImGui::CloseCurrentPopup();
				if (continuation) continuation();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Discard and Continue")) {
			RevertAssetEdit();
			auto continuation = std::move(m_DirtyAssetContinuation);
			ImGui::CloseCurrentPopup();
			if (continuation) continuation();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) {
			m_DirtyAssetContinuation = {};
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	void InspectorPanel::SetAssetRecords(std::span<const AssetRecord> records) {
		m_MeshAssetOptions = Editor::BuildAssetPickerOptions(records, AssetKind::Mesh);
		m_MaterialAssetOptions = Editor::BuildAssetPickerOptions(records, AssetKind::Material);
		m_TextureAssetOptions = Editor::BuildAssetPickerOptions(records, AssetKind::Texture2D);
		m_ShaderAssetOptions = Editor::BuildAssetPickerOptions(records, AssetKind::Shader);
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
		if (Selection::HasAssetSelection()) {
			const auto guid = Selection::GetSelectedAssetGuid();
			if (!m_AssetInspectorHost.GetSession().IsOpen() || m_AssetInspectorHost.GetSession().GetGuid() != guid) {
				auto result = m_AssetInspectorHost.Open(guid, [](const AssetGuid& assetGuid, AssetInspectionSnapshot& snapshot) {
					return Application::GetInstance().GetOperations().InspectAsset(assetGuid, snapshot);
				});
				if (!result.Succeeded() && m_WorkbenchState) m_WorkbenchState->RecordEvent(result, "Inspector");
			}
			if (auto* editor = m_AssetInspectorHost.GetEditor()) {
				const bool dirty = editor->IsDirty();
				ImGui::BeginDisabled(!dirty || !m_ProjectContext);
				if (ImGui::Button("Apply")) {
					(void)ApplyAssetEdit();
				}
				ImGui::EndDisabled();
				ImGui::SameLine();
				ImGui::BeginDisabled(!dirty);
				if (ImGui::Button("Revert")) RevertAssetEdit();
				ImGui::EndDisabled();
				ImGui::Separator();
				Editor::AssetEditorDrawContext context{
					.ShaderAssets = m_ShaderAssetOptions,
					.TextureAssets = m_TextureAssetOptions,
					.GetShaderAuthoringMetadata = [](const AssetGuid& shaderGuid, Rendering::ShaderAuthoringMetadata& metadata) { return Application::GetInstance().GetOperations().GetShaderAuthoringMetadata(shaderGuid, metadata); },
					.ReimportAsset = [this](const std::filesystem::path& path) {
						AssetReimportReport report;
						auto result = m_ProjectContext
							? Application::GetInstance().GetOperations().ReimportAssets(*m_ProjectContext, path, &report)
							: ResultEnvelope::Failure("asset.reimport", path.generic_string(), "Project context is unavailable");
						if (m_WorkbenchState) m_WorkbenchState->RecordEvent(result, "Inspector");
						return result;
					}
				};
				editor->Draw(context);
			}
			else {
				ImGui::TextDisabled("Asset inspector unavailable.");
			}
		}
		else if (Selection::HasSelection()) {
			m_AssetInspectorHost.Close();
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
								, .TextureAssets = m_TextureAssetOptions
								, .ResolveMaterialDefinition = [](const AssetGuid& guid, Rendering::MaterialDefinition& definition, AssetImportHealth& health) { return Application::GetInstance().GetOperations().GetMaterialDefinition(guid, definition, &health); }
								, .CommitMaterialOverrides = [this, selection](const Rendering::MaterialOverrideSet& overrides) {
									if (!m_InteractionHost || !selection.IsValid() || !selection.HasComponent<Rendering::MaterialComponent>()) return;
									const auto before = selection.GetComponent<Rendering::MaterialComponent>().Overrides;
									(void)m_InteractionHost->ExecuteCommand(CreateSetMaterialOverridesCommand(selection, before, overrides));
								}
								, .CommitMaterialReference = [this, selection](const AssetGuid& guid) {
									if (!m_InteractionHost || !selection.IsValid() || !selection.HasComponent<Rendering::MaterialComponent>()) return;
									const auto before = selection.GetComponent<Rendering::MaterialComponent>();
									auto after = before; after.Material.Reference.Guid = guid;
									Rendering::MaterialDefinition definition;
									const bool overridesRemoved = Application::GetInstance().GetOperations().GetMaterialDefinition(guid, definition).Succeeded() &&
										Rendering::ReconcileMaterialOverrides(after.Overrides, definition);
									const auto commandResult = m_InteractionHost->ExecuteCommand(CreateSetMaterialComponentCommand(selection, before, after));
									if (commandResult.Succeeded() && overridesRemoved && m_WorkbenchState) {
										auto result = ResultEnvelope::Success(
											"editor.material_overrides.reconciled",
											guid,
											"Incompatible material overrides were removed");
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
		}
		else {
			m_AssetInspectorHost.Close();
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
