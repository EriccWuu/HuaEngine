#include "enginepch.h"
#include "Scene/SceneEntityInspectorEditor.h"
#include "HuaEngine/Application.h"
#include "HuaEngine/Application/ApplicationOperations.h"
#include "HuaEngine/Application/EcsResultEnvelope.h"
#include "HuaEngine/ECS/Runtime/WorldScope.h"
#include "Interaction/EditorInteractionHost.h"
#include "Selection.h"
#include "Workbench/EditorWorkbenchState.h"
#include "Workbench/SceneDocument.h"
#include "imgui.h"
#include <algorithm>

namespace HE::Editor {
    namespace {
        const EditorInspectableComponentDescriptor* FindInspectableComponentByRuntimeType(std::string_view name) {
            if (name == "HE::Rendering::CameraComponent") return FindEditorInspectableComponent("component.camera");
            if (name == "HE::Rendering::MeshComponent") return FindEditorInspectableComponent("component.mesh");
            if (name == "HE::Rendering::MaterialComponent") return FindEditorInspectableComponent("component.material");
            return nullptr;
        }
        std::string Header(const Ecs::RegisteredType& type) {
            if (type.Descriptor.Reflection) {
                auto display = GetRuntimeComponentDisplayName(*type.Descriptor.Reflection);
                if (!display.empty()) return display;
            }
            return type.Descriptor.Name;
        }
        struct ComponentEdit {
            const Ecs::RegisteredType* Type = nullptr;
            Ecs::OwnedValue Before;
            Ecs::OwnedValue After;
        };
    }
    SceneEntityInspectorEditor::SceneEntityInspectorEditor(const AssetPickerCatalog& catalog) : m_PickerCatalog(catalog) {}
    bool SceneEntityInspectorEditor::HasEditingContext() const {
        return m_InteractionHost && m_InteractionHost->GetSceneDocument() && m_InteractionHost->GetSceneDocument()->SceneRef;
    }
    const Ecs::TypeRegistry* SceneEntityInspectorEditor::ResolveTypeRegistry() const {
        return HasEditingContext() ? &m_InteractionHost->GetSceneDocument()->SceneRef->GetWorld().Types() : nullptr;
    }
    bool SceneEntityInspectorEditor::Draw() {
        if (m_InteractionHost && m_InteractionHost->HasActiveScene()) m_InteractionHost->Commands().SetLastRoute("panel.inspector");
        if (!HasEditingContext()) { ImGui::TextUnformatted("No entity selected."); return false; }
        // Keep the registry alive while copied component values borrow its descriptors.
        const auto scene = m_InteractionHost->GetSceneDocument()->SceneRef;
        auto& world = scene->GetWorld();
        if (!Selection::HasSingleSelection()) {
            const auto selections = Selection::ResolveSelections(world);
            ImGui::Text("%zu entities selected", selections.size());
            ImGui::TextDisabled("Multi-selection is currently summary-only.");
            ImGui::Separator();
            const auto count = (std::min)(selections.size(), size_t{6});
            for (size_t index = 0; index < count; ++index) ImGui::BulletText("%s", std::string(world.Name(selections[index])).c_str());
            if (selections.size() > count) ImGui::TextDisabled("...and %zu more", selections.size() - count);
            DrawAddComponentWindow();
            return false;
        }
        const auto entity = Selection::ResolvePrimarySelection(world);
        if (!world.IsAlive(entity)) { Selection::ClearSelection(); ImGui::TextUnformatted("No entity selected."); return false; }
        std::vector<ComponentEdit> edits;
        {
            auto scope = Ecs::WorldReadScope::Acquire(world);
            if (!scope) { ImGui::TextDisabled("Entity is busy: %s", scope.GetError().Message.c_str()); return false; }
            const auto& read = scope.Value().Get();
            for (auto id : read.ListTypes(entity)) {
                const auto* type = read.Types().Find(id);
                if (!type) continue;
                ComponentEdit edit;
                edit.Type = type;
                if (type->Descriptor.Reflection && type->Descriptor.Storage != Ecs::StorageKind::Tag) {
                    auto before = Ecs::OwnedValue::Copy(*type, read.TryGet(entity, id));
                    if (before) {
                        auto after = before.Value().Clone();
                        if (after) { edit.Before = std::move(before).Value(); edit.After = std::move(after).Value(); }
                    }
                }
                edits.push_back(std::move(edit));
            }
        }
        bool changed = false;
        ImGui::PushID(static_cast<int>(entity.Index));
        ImGui::TextUnformatted(std::string(world.Name(entity)).c_str());
        if (ImGui::BeginPopupContextWindow("InspectorEntityContextMenu")) {
            if (ImGui::MenuItem("Add Component...")) { RequestOpenAddComponentWindow(); ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }
        for (auto& edit : edits) {
            const auto* reflection = edit.Type->Descriptor.Reflection;
            ImGui::PushID(static_cast<int>(edit.Type->Id));
            const bool open = ImGui::TreeNodeEx(Header(*edit.Type).c_str(), ImGuiTreeNodeFlags_DefaultOpen);
            bool removeRequested = false;
            if (reflection && ImGui::BeginPopupContextItem("ComponentContextMenu")) {
                const auto* descriptor = FindInspectableComponentByRuntimeType(reflection->QualifiedName);
                const bool canRemove = descriptor && CanRemoveInspectableComponent(descriptor->Type, world, entity);
                if (ImGui::MenuItem("Remove Component", nullptr, false, canRemove) && m_RemoveComponentCallback) {
                    m_RemoveComponentCallback(descriptor->Type);
                    removeRequested = true;
                }
                if (!canRemove && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Runtime remove command is not available for this component.");
                ImGui::EndPopup();
            }
            if (open) {
                if (!reflection) ImGui::TextDisabled("Runtime descriptor is not available.");
                else if (!edit.After) ImGui::TextDisabled("This component does not support editable value snapshots.");
                else if (!removeRequested && world.IsAlive(entity) && world.Has(entity, edit.Type->Id)) {
                    std::vector<ResultEnvelope> deferredEvents;
                    const bool fieldChanged = DrawRuntimeFields(*reflection, edit.After.Data(), m_FieldDrawers, {
                        .MeshAssets = m_PickerCatalog.Get(AssetKind::Mesh),
                        .MaterialAssets = m_PickerCatalog.Get(AssetKind::Material),
                        .TextureAssets = m_PickerCatalog.Get(AssetKind::Texture2D),
                        .ResolveMaterialDefinition = [](const AssetGuid& guid, Rendering::MaterialDefinition& definition, AssetImportHealth& health) {
                            return Application::GetInstance().GetOperations().GetMaterialDefinition(guid, definition, &health);
                        },
                        .DeferredEvents = &deferredEvents
                    });
                    if (fieldChanged) {
                        auto result = m_InteractionHost->ExecuteCommand(CreateSetComponentValueCommand(scene, entity, std::move(edit.Before), std::move(edit.After)));
                        changed |= result.Succeeded();
                        if (result.Succeeded() && m_WorkbenchState) {
                            for (const auto& event : deferredEvents) m_WorkbenchState->RecordEvent(event, "Inspector");
                        }
                    }
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
        if (!ImGui::Begin("Add Component", &m_ShowAddComponentWindow, ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }
        if (!HasEditingContext() || !Selection::HasSingleSelection()) { ImGui::TextDisabled("Select a single entity to add components."); ImGui::End(); return; }
        const auto scene = m_InteractionHost->GetSceneDocument()->SceneRef;
        auto& world = scene->GetWorld();
        const auto entity = Selection::ResolvePrimarySelection(world);
        if (!world.IsAlive(entity)) { ImGui::End(); return; }
        for (const auto* type : world.Types().All()) {
            const bool exists = world.Has(entity, type->Id);
            const auto* descriptor = type->Descriptor.Reflection ? FindInspectableComponentByRuntimeType(type->Descriptor.Reflection->QualifiedName) : nullptr;
            const bool canAdd = !exists && descriptor && m_AddComponentCallback;
            const auto display = Header(*type);
            if (ImGui::Selectable(display.c_str(), false, canAdd ? 0 : ImGuiSelectableFlags_Disabled) && canAdd) {
                m_AddComponentCallback(descriptor->Type);
                m_ShowAddComponentWindow = false;
            }
            if (exists) { ImGui::SameLine(); ImGui::TextDisabled("(Already Added)"); }
            else if (!descriptor) { ImGui::SameLine(); ImGui::TextDisabled("(Runtime command unavailable)"); }
        }
        ImGui::End();
    }
}
