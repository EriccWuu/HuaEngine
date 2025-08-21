#pragma once

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Scene/Scene.h"
#include "imgui.h"
#include "glm/glm.hpp"
#include "ComponentEditorRegistry.h"

namespace HE {
	class InspectorPanel {
	public:
		InspectorPanel() = default;
		~InspectorPanel() = default;

		void OnGuiRender();
		void SetSelection(const Ref<Entity>& target) { m_Selection = target; }

	private:
		Ref<Entity> m_Selection;
	};
}