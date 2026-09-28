#include "enginepch.h"
#include "Selection.h"
#include <algorithm>

namespace HE {
    Editor::EditorSelectionService& Selection::GetService() {
        static Editor::EditorSelectionService service;
        return service;
    }
    void Selection::SetSelection(const Ecs::World& world, EntityId entity) {
        SetSelectedEntity(world.IsAlive(entity) ? world.Uuid(entity) : EntityUuid{});
    }
    void Selection::SetSelectedEntity(EntityUuid uuid) {
        GetService().SelectEntities(uuid == EntityUuid{} ? std::vector<EntityUuid>{} : std::vector<EntityUuid>{uuid});
    }
    void Selection::SetSelections(const Ecs::World& world, std::span<const EntityId> entities) {
        std::vector<EntityUuid> selected;
        selected.reserve(entities.size());
        for (auto entity : entities) if (world.IsAlive(entity)) selected.push_back(world.Uuid(entity));
        SetSelectedEntities(std::move(selected));
    }
    void Selection::SetSelectedEntities(std::vector<EntityUuid> entities) { GetService().SelectEntities(std::move(entities)); }
    void Selection::AddToSelection(const Ecs::World& world, EntityId entity) {
        if (!world.IsAlive(entity) || IsSelected(world, entity)) return;
        auto entities = GetSelectedEntityUuids();
        entities.push_back(world.Uuid(entity));
        SetSelectedEntities(std::move(entities));
    }
    void Selection::ToggleSelection(const Ecs::World& world, EntityId entity) {
        if (!world.IsAlive(entity)) return;
        if (IsSelected(world, entity)) RemoveFromSelection(world, entity); else AddToSelection(world, entity);
    }
    void Selection::RemoveFromSelection(const Ecs::World& world, EntityId entity) {
        auto entities = GetSelectedEntityUuids();
        const auto uuid = world.Uuid(entity);
        std::erase(entities, uuid);
        SetSelectedEntities(std::move(entities));
    }
    EntityId Selection::ResolvePrimarySelection(const Ecs::World& world) {
        RemoveInvalidSelections(world);
        const auto& entities = GetSelectedEntityUuids();
        return entities.empty() ? EntityId{} : world.Find(entities.front());
    }
    std::vector<EntityId> Selection::ResolveSelections(const Ecs::World& world) {
        RemoveInvalidSelections(world);
        std::vector<EntityId> result;
        result.reserve(GetSelectedEntityUuids().size());
        for (const auto uuid : GetSelectedEntityUuids()) {
            const auto entity = world.Find(uuid);
            if (world.IsAlive(entity)) result.push_back(entity);
        }
        return result;
    }
    EntityUuid Selection::GetSelectedEntityUuid() {
        const auto& entities = GetSelectedEntityUuids();
        return entities.empty() ? EntityUuid{} : entities.front();
    }
    const std::vector<EntityUuid>& Selection::GetSelectedEntityUuids() {
        static const std::vector<EntityUuid> empty;
        const auto* selected = GetService().GetEntitySelection();
        return selected ? selected->Entities : empty;
    }
    bool Selection::HasSelection() { RemoveInvalidSelections(); return GetService().HasEntitySelection(); }
    bool Selection::HasSingleSelection() { return GetSelectedEntityUuids().size() == 1; }
    bool Selection::IsSelected(const Ecs::World& world, EntityId entity) {
        if (!world.IsAlive(entity)) return false;
        const auto& entities = GetSelectedEntityUuids();
        return std::find(entities.begin(), entities.end(), world.Uuid(entity)) != entities.end();
    }
    size_t Selection::Count() { return GetSelectedEntityUuids().size(); }
    void Selection::ClearSelection() { GetService().Clear(); }
    void Selection::RemoveInvalidSelections() {
        if (const auto* selected = GetService().GetEntitySelection(); selected && selected->Entities.empty()) GetService().Clear();
    }
    void Selection::RemoveInvalidSelections(const Ecs::World& world) {
        if (!GetService().HasEntitySelection()) return;
        auto entities = GetSelectedEntityUuids();
        std::erase_if(entities, [&world](EntityUuid uuid) { return !world.IsAlive(world.Find(uuid)); });
        SetSelectedEntities(std::move(entities));
    }
    void Selection::SelectAsset(AssetGuid guid) { GetService().SelectAsset(std::move(guid)); }
    bool Selection::HasAssetSelection() { return GetService().HasAssetSelection(); }
    AssetGuid Selection::GetSelectedAssetGuid() {
        const auto* selected = GetService().GetAssetSelection();
        return selected ? selected->Guid : AssetGuid{};
    }
}
