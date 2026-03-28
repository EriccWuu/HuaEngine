#pragma once

#include <functional>

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Scene/Scene.h"
#include "Workbench/EditorWorkbenchState.h"
#include "Interaction/EditorSceneCommands.h"
#include "imgui.h"
#include "glm/glm.hpp"
#include "ComponentEditorRegistry.h"

namespace HE {
    class EditorInteractionHost;

	class InspectorPanel {
	public:
		InspectorPanel() = default;
		~InspectorPanel() = default;

		bool OnGuiRender();
		void SetWorkbenchState(const EditorWorkbenchState* state) { m_WorkbenchState = state; }
		void SetSelection(const Ref<Entity>& target) { m_Selection = target; }
        void SetInteractionHost(EditorInteractionHost* host) { m_InteractionHost = host; }
        void SetAddComponentCallback(std::function<void(EditorInspectableComponent)> callback) { m_AddComponentCallback = std::move(callback); }
        void SetRemoveComponentCallback(std::function<void(EditorInspectableComponent)> callback) { m_RemoveComponentCallback = std::move(callback); }

	private:
        bool DrawRegisteredContextMenu(std::string_view contextId);
        void DrawAddComponentWindow();
        void RequestOpenAddComponentWindow() { m_ShowAddComponentWindow = true; }

		Ref<Entity> m_Selection;
		const EditorWorkbenchState* m_WorkbenchState = nullptr;
        EditorInteractionHost* m_InteractionHost = nullptr;
        std::function<void(EditorInspectableComponent)> m_AddComponentCallback;
        std::function<void(EditorInspectableComponent)> m_RemoveComponentCallback;
        bool m_ShowAddComponentWindow = false;
	};
}
