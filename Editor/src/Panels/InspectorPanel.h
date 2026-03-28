#pragma once

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Scene/Scene.h"
#include "Workbench/EditorWorkbenchState.h"
#include "imgui.h"
#include "glm/glm.hpp"
#include "ComponentEditorRegistry.h"

namespace HE {
	class InspectorPanel {
	public:
		InspectorPanel() = default;
		~InspectorPanel() = default;

		bool OnGuiRender();
		void SetWorkbenchState(const EditorWorkbenchState* state) { m_WorkbenchState = state; }
		void SetSelection(const Ref<Entity>& target) { m_Selection = target; }

	private:
		Ref<Entity> m_Selection;
		const EditorWorkbenchState* m_WorkbenchState = nullptr;
	};
}
