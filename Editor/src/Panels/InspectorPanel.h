#pragma once

#include <functional>
#include <span>
#include <vector>

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/ECS/ComponentRegistry.h"
#include "HuaEngine/Scene/Scene.h"
#include "Workbench/EditorWorkbenchState.h"
#include "Interaction/EditorSceneCommands.h"
#include "Panels/RuntimeInspector.h"
#include "imgui.h"
#include "glm/glm.hpp"

namespace HE {
    class EditorInteractionHost;

	class InspectorPanel {
	public:
		InspectorPanel();
		~InspectorPanel() = default;

		bool OnGuiRender();
		void SetWorkbenchState(const EditorWorkbenchState* state) { m_WorkbenchState = state; }
		void SetSelection(const Ref<Entity>& target) { m_Selection = target; }
        void SetInteractionHost(EditorInteractionHost* host) { m_InteractionHost = host; }
        void SetAddComponentCallback(std::function<void(EditorInspectableComponent)> callback) { m_AddComponentCallback = std::move(callback); }
		void SetRemoveComponentCallback(std::function<void(EditorInspectableComponent)> callback) { m_RemoveComponentCallback = std::move(callback); }
		void SetAssetRecords(std::span<const AssetRecord> records);
		void ClearAssetRecords() {
			m_MeshAssetOptions.clear();
			m_MaterialAssetOptions.clear();
		}

	private:
        bool DrawRegisteredContextMenu(std::string_view contextId);
        void DrawAddComponentWindow();
        void RequestOpenAddComponentWindow() { m_ShowAddComponentWindow = true; }

		Ref<Entity> m_Selection;
		const EditorWorkbenchState* m_WorkbenchState = nullptr;
        EditorInteractionHost* m_InteractionHost = nullptr;
        std::function<void(EditorInspectableComponent)> m_AddComponentCallback;
        std::function<void(EditorInspectableComponent)> m_RemoveComponentCallback;
        ComponentRegistry m_ComponentRegistry;
        Editor::RuntimeComponentEditorOverrideRegistry m_RuntimeOverrides;
		std::vector<Editor::AssetPickerOption> m_MeshAssetOptions;
		std::vector<Editor::AssetPickerOption> m_MaterialAssetOptions;
        bool m_ShowAddComponentWindow = false;
	};
}
