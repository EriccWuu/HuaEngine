#pragma once

#include <vector>

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/ECS/Entity.h"
#include "HuaEngine/ECS/EntityId.h"
#include "HuaEngine/ECS/World.h"
#include "Selection/EditorSelectionService.h"

namespace HE {
	class Selection {
	public:
		static void SetSelection(const Entity& selection);
		static void SetSelectedEntity(EntityUuid uuid);
		static void SetSelections(std::vector<Entity> selections);
		static void SetSelectedEntities(std::vector<EntityUuid> selections);
		static void AddToSelection(const Entity& selection);
		static void ToggleSelection(const Entity& selection);
		static void RemoveFromSelection(const Entity& selection);
		static Entity& GetSelection();
		static Entity& GetPrimarySelection() { return GetSelection(); }
		static const std::vector<Entity>& GetSelections();
		static Entity ResolvePrimarySelection(World& world);
		static const std::vector<Entity>& ResolveSelections(World& world);
		static EntityUuid GetSelectedEntityUuid();
		static const std::vector<EntityUuid>& GetSelectedEntityUuids();
		static bool HasSelection();
		static bool HasSingleSelection();
		static bool IsSelected(const Entity& selection);
		static size_t Count();
		static void ClearSelection();
		static void RemoveInvalidSelections();
		static void RemoveInvalidSelections(World& world);
		static void SelectAsset(AssetGuid guid);
		static bool HasAssetSelection();
		static AssetGuid GetSelectedAssetGuid();
		static Editor::EditorSelectionService& GetService();
	};
}
