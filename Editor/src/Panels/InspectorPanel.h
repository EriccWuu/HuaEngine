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

	private:
		Editor::AssetInspectorEditor& m_AssetEditor;
		Editor::SceneEntityInspectorEditor& m_SceneEditor;
	};
}
