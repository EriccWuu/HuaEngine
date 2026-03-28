#pragma once

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/ECS/Entity.h"

namespace HE {
	class Selection {
	public:
		static void SetSelection(const Entity& selection) { m_Selection = selection; }
		static Entity& GetSelection() { return m_Selection; }
		static bool HasSelection() { return m_Selection.IsValid(); }
		static void ClearSelection() { m_Selection = {}; }
	private:
		static Entity m_Selection;
	};
}
