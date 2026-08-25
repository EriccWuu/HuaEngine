#include "enginepch.h"
#include "Selection/EditorSelectionService.h"

#include <algorithm>

namespace HE::Editor {
	const EntitySelection* EditorSelectionService::GetEntitySelection() const {
		return std::get_if<EntitySelection>(&m_Selection);
	}

	const AssetSelection* EditorSelectionService::GetAssetSelection() const {
		return std::get_if<AssetSelection>(&m_Selection);
	}

	void EditorSelectionService::SelectEntities(std::vector<EntityUuid> entities) {
		entities.erase(
			std::remove(entities.begin(), entities.end(), EntityUuid{}),
			entities.end());
		if (entities.empty()) {
			Clear();
			return;
		}
		(void)TrySelect(EntitySelection{ std::move(entities) });
	}

	void EditorSelectionService::SelectAsset(AssetGuid guid) {
		if (guid.empty()) {
			Clear();
			return;
		}
		(void)TrySelect(AssetSelection{ std::move(guid) });
	}

	void EditorSelectionService::Clear() {
		(void)TrySelect(NoEditorSelection{});
	}

	bool EditorSelectionService::TrySelect(EditorSelection selection) {
		if (m_ChangeGuard && !m_ChangeGuard(selection)) return false;
		m_Selection = std::move(selection);
		return true;
	}

	bool EditorSelectionService::HasSelection() const {
		return !std::holds_alternative<NoEditorSelection>(m_Selection);
	}

	bool EditorSelectionService::HasEntitySelection() const {
		return std::holds_alternative<EntitySelection>(m_Selection);
	}

	bool EditorSelectionService::HasAssetSelection() const {
		return std::holds_alternative<AssetSelection>(m_Selection);
	}
}
