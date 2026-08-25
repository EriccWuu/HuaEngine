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
		~InspectorPanel();

		bool OnGuiRender();
		void OnDirtyAssetPopup();
		[[nodiscard]] bool HasDirtyAsset() const;
		[[nodiscard]] ResultEnvelope ApplyAssetEdit();
		void RevertAssetEdit();
		bool RequestDirtyAssetResolution(std::function<void()> continuation);
		void CheckExternalAssetModification();
		void SetWorkbenchState(EditorWorkbenchState* state) { m_WorkbenchState = state; }
        void SetInteractionHost(EditorInteractionHost* host) { m_InteractionHost = host; }
        void SetAddComponentCallback(std::function<void(EditorInspectableComponent)> callback) { m_AddComponentCallback = std::move(callback); }
		void SetRemoveComponentCallback(std::function<void(EditorInspectableComponent)> callback) { m_RemoveComponentCallback = std::move(callback); }
		void SetAssetRecords(std::span<const AssetRecord> records);
		void SetProjectContext(const ProjectContext* context) { m_ProjectContext = context; }
		void SetOpenSceneCallback(std::function<void(const std::filesystem::path&)> callback) { m_OpenSceneCallback = std::move(callback); }
		void ClearAssetRecords() {
			m_MeshAssetOptions.clear();
			m_MaterialAssetOptions.clear();
			m_TextureAssetOptions.clear();
			m_ShaderAssetOptions.clear();
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
		std::vector<Editor::AssetPickerOption> m_ShaderAssetOptions;
		std::function<void()> m_DirtyAssetContinuation;
		std::function<void(const std::filesystem::path&)> m_OpenSceneCallback;
		bool m_OpenDirtyAssetPopup = false;
        bool m_ShowAddComponentWindow = false;
	};
}
