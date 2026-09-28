#pragma once
#include <span>
#include <initializer_list>
#include <vector>
#include "HuaEngine/ECS/Runtime/World.h"
#include "Selection/EditorSelectionService.h"

namespace HE {
    class Selection {
    public:
        static void SetSelection(const Ecs::World& world, EntityId entity);
        static void SetSelectedEntity(EntityUuid uuid);
        static void SetSelections(const Ecs::World& world, std::span<const EntityId> entities);
        static void SetSelections(const Ecs::World& world, std::initializer_list<EntityId> entities) {
            SetSelections(world, std::span<const EntityId>{entities.begin(), entities.size()});
        }
        static void SetSelectedEntities(std::vector<EntityUuid> selections);
        static void AddToSelection(const Ecs::World& world, EntityId entity);
        static void ToggleSelection(const Ecs::World& world, EntityId entity);
        static void RemoveFromSelection(const Ecs::World& world, EntityId entity);
        static EntityId ResolvePrimarySelection(const Ecs::World& world);
        static std::vector<EntityId> ResolveSelections(const Ecs::World& world);
        static EntityUuid GetSelectedEntityUuid();
        static const std::vector<EntityUuid>& GetSelectedEntityUuids();
        static bool HasSelection();
        static bool HasSingleSelection();
        static bool IsSelected(const Ecs::World& world, EntityId entity);
        static size_t Count();
        static void ClearSelection();
        static void RemoveInvalidSelections();
        static void RemoveInvalidSelections(const Ecs::World& world);
        static void SelectAsset(AssetGuid guid);
        static bool HasAssetSelection();
        static AssetGuid GetSelectedAssetGuid();
        static Editor::EditorSelectionService& GetService();
    };
}
