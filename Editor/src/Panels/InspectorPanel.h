#pragma once

#include <functional>
#include "Assets/AssetInspectorEditor.h"
#include "Assets/AssetPickerCatalog.h"
#include "HuaEngine/ECS/ComponentRegistry.h"
#include "Workbench/EditorWorkbenchState.h"
#include "Interaction/EditorSceneCommands.h"
#include "Panels/RuntimeInspector.h"

namespace HE {
    class EditorInteractionHost;

	class InspectorPanel {
	public:
		InspectorPanel(Editor::AssetInspectorEditor& assetEditor, const Editor::AssetPickerCatalog& pickerCatalog);

		bool OnGuiRender();
		void SetWorkbenchState(EditorWorkbenchState* state) { m_WorkbenchState = state; }
        void SetInteractionHost(EditorInteractionHost* host) { m_InteractionHost = host; }
        void SetAddComponentCallback(std::function<void(EditorInspectableComponent)> callback) { m_AddComponentCallback = std::move(callback); }
		void SetRemoveComponentCallback(std::function<void(EditorInspectableComponent)> callback) { m_RemoveComponentCallback = std::move(callback); }

	private:
        void DrawAddComponentWindow();
        void RequestOpenAddComponentWindow() { m_ShowAddComponentWindow = true; }

		EditorWorkbenchState* m_WorkbenchState = nullptr;
        EditorInteractionHost* m_InteractionHost = nullptr;
        std::function<void(EditorInspectableComponent)> m_AddComponentCallback;
        std::function<void(EditorInspectableComponent)> m_RemoveComponentCallback;
        ComponentRegistry m_ComponentRegistry;
        Editor::RuntimeComponentEditorOverrideRegistry m_RuntimeOverrides;
		Editor::AssetInspectorEditor& m_AssetEditor;
		const Editor::AssetPickerCatalog& m_PickerCatalog;
        bool m_ShowAddComponentWindow = false;
	};
}
