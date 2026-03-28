#pragma once

#include <vector>

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/ECS/Entity.h"

namespace HE {
	class Selection {
	public:
		static void SetSelection(const Entity& selection);
		static void SetSelections(std::vector<Entity> selections);
		static void AddToSelection(const Entity& selection);
		static void ToggleSelection(const Entity& selection);
		static void RemoveFromSelection(const Entity& selection);
		static Entity& GetSelection();
		static Entity& GetPrimarySelection() { return GetSelection(); }
		static const std::vector<Entity>& GetSelections();
		static bool HasSelection();
		static bool HasSingleSelection();
		static bool IsSelected(const Entity& selection);
		static size_t Count();
		static void ClearSelection();
		static void RemoveInvalidSelections();
	private:
		static std::vector<Entity> m_Selections;
		static Entity m_EmptySelection;
	};
}
