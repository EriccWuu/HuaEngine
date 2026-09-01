#pragma once

#include "Assets/AssetInspectorEditor.h"
#include "Scene/SceneEntityInspectorEditor.h"

namespace HE {
	class InspectorPanel {
	public:
		InspectorPanel(
			Editor::AssetInspectorEditor& assetEditor,
			Editor::SceneEntityInspectorEditor& sceneEditor)
			: m_AssetEditor(assetEditor), m_SceneEditor(sceneEditor) {}

		bool OnGuiRender();
		[[nodiscard]] bool IsFocused() const { return m_IsFocused; }
		[[nodiscard]] bool IsHovered() const { return m_IsHovered; }

	private:
		Editor::AssetInspectorEditor& m_AssetEditor;
		Editor::SceneEntityInspectorEditor& m_SceneEditor;
		bool m_IsFocused = false;
		bool m_IsHovered = false;
	};
}
