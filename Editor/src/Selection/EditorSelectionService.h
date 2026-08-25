#pragma once

#include <functional>
#include <span>

#include "Selection/EditorSelectionTypes.h"

namespace HE::Editor {
	class EditorSelectionService {
	public:
		using ChangeGuard = std::function<bool(const EditorSelection&)>;

		[[nodiscard]] const EditorSelection& GetSelection() const { return m_Selection; }
		[[nodiscard]] const EntitySelection* GetEntitySelection() const;
		[[nodiscard]] const AssetSelection* GetAssetSelection() const;

		void SelectEntities(std::vector<EntityUuid> entities);
		void SelectAsset(AssetGuid guid);
		void Clear();
		void SetChangeGuard(ChangeGuard guard) { m_ChangeGuard = std::move(guard); }
		void AcceptGuardedSelection(EditorSelection selection) { m_Selection = std::move(selection); }

		[[nodiscard]] bool HasSelection() const;
		[[nodiscard]] bool HasEntitySelection() const;
		[[nodiscard]] bool HasAssetSelection() const;

	private:
		bool TrySelect(EditorSelection selection);

		EditorSelection m_Selection = NoEditorSelection{};
		ChangeGuard m_ChangeGuard;
	};
}
