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
#include "Assets/AssetInspectorHost.h"
#include "imgui.h"
#include "glm/glm.hpp"

namespace HE {
    class EditorInteractionHost;

	class InspectorPanel {
	public:
		InspectorPanel();
		~InspectorPanel() = default;

		bool OnGuiRender();
		void SetWorkbenchState(EditorWorkbenchState* state) { m_WorkbenchState = state; }
        void SetInteractionHost(EditorInteractionHost* host) { m_InteractionHost = host; }
        void SetAddComponentCallback(std::function<void(EditorInspectableComponent)> callback) { m_AddComponentCallback = std::move(callback); }
		void SetRemoveComponentCallback(std::function<void(EditorInspectableComponent)> callback) { m_RemoveComponentCallback = std::move(callback); }
		void SetAssetRecords(std::span<const AssetRecord> records);
		void SetProjectContext(const ProjectContext* context) { m_ProjectContext = context; }
		void ClearAssetRecords() {
			m_MeshAssetOptions.clear();
			m_MaterialAssetOptions.clear();
			m_TextureAssetOptions.clear();
		}

	private:
        bool DrawRegisteredContextMenu(std::string_view contextId);
        void DrawAddComponentWindow();
        void RequestOpenAddComponentWindow() { m_ShowAddComponentWindow = true; }

		EditorWorkbenchState* m_WorkbenchState = nullptr;
		const ProjectContext* m_ProjectContext = nullptr;
        EditorInteractionHost* m_InteractionHost = nullptr;
        std::function<void(EditorInspectableComponent)> m_AddComponentCallback;
        std::function<void(EditorInspectableComponent)> m_RemoveComponentCallback;
        ComponentRegistry m_ComponentRegistry;
        Editor::RuntimeComponentEditorOverrideRegistry m_RuntimeOverrides;
		Editor::AssetInspectorHost m_AssetInspectorHost;
		std::vector<Editor::AssetPickerOption> m_MeshAssetOptions;
		std::vector<Editor::AssetPickerOption> m_MaterialAssetOptions;
		std::vector<Editor::AssetPickerOption> m_TextureAssetOptions;
        bool m_ShowAddComponentWindow = false;
	};
}
