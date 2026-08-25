#pragma once

#include <functional>

#include "Assets/AssetPickerCatalog.h"
#include "HuaEngine/ECS/ComponentRegistry.h"
#include "Interaction/EditorSceneCommands.h"
#include "Panels/RuntimeInspector.h"

namespace HE {
	class EditorInteractionHost;
	class EditorWorkbenchState;
}

namespace HE::Editor {
	class SceneEntityInspectorEditor {
	public:
		explicit SceneEntityInspectorEditor(const AssetPickerCatalog& pickerCatalog);

		bool Draw();
		[[nodiscard]] bool HasEditingContext() const;

		void BindInteractionHost(EditorInteractionHost* host) { m_InteractionHost = host; }
		void SetWorkbenchState(EditorWorkbenchState* state) { m_WorkbenchState = state; }
		void SetAddComponentCallback(std::function<void(EditorInspectableComponent)> callback) { m_AddComponentCallback = std::move(callback); }
		void SetRemoveComponentCallback(std::function<void(EditorInspectableComponent)> callback) { m_RemoveComponentCallback = std::move(callback); }

	private:
		void DrawAddComponentWindow();
		void RequestOpenAddComponentWindow() { m_ShowAddComponentWindow = true; }

		const AssetPickerCatalog& m_PickerCatalog;
		EditorInteractionHost* m_InteractionHost = nullptr;
		EditorWorkbenchState* m_WorkbenchState = nullptr;
		std::function<void(EditorInspectableComponent)> m_AddComponentCallback;
		std::function<void(EditorInspectableComponent)> m_RemoveComponentCallback;
		ComponentRegistry m_ComponentRegistry;
		RuntimeComponentEditorOverrideRegistry m_RuntimeOverrides;
		bool m_ShowAddComponentWindow = false;
	};
}
