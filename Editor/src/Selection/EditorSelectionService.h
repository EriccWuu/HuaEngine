#pragma once

#include <span>

#include "Selection/EditorSelectionTypes.h"

namespace HE::Editor {
	class EditorSelectionService {
	public:
		[[nodiscard]] const EditorSelection& GetSelection() const { return m_Selection; }
		[[nodiscard]] const EntitySelection* GetEntitySelection() const;
		[[nodiscard]] const AssetSelection* GetAssetSelection() const;

		void SelectEntities(std::vector<EntityUuid> entities);
		void SelectAsset(AssetGuid guid);
		void Clear();

		[[nodiscard]] bool HasSelection() const;
		[[nodiscard]] bool HasEntitySelection() const;
		[[nodiscard]] bool HasAssetSelection() const;

	private:
		EditorSelection m_Selection = NoEditorSelection{};
	};
}
