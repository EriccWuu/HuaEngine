#pragma once

#include <functional>
#include <memory>

#include "Assets/AssetEditorRegistry.h"
#include "Assets/Editors/SceneAssetEditor.h"

namespace HE::Editor {
	using InspectAssetCallback = std::function<ResultEnvelope(const AssetGuid&, AssetInspectionSnapshot&)>;

	class AssetInspectorHost {
	public:
		explicit AssetInspectorHost(SceneAssetEditorServices sceneServices = {});

		[[nodiscard]] ResultEnvelope Open(const AssetGuid& guid, const InspectAssetCallback& inspectAsset);
		void Close();

		[[nodiscard]] const AssetEditSession& GetSession() const { return m_Session; }
		[[nodiscard]] AssetEditSession& GetSession() { return m_Session; }
		[[nodiscard]] IAssetEditor* GetEditor() const { return m_Editor.get(); }
		[[nodiscard]] AssetEditorRegistry& GetRegistry() { return m_Registry; }

	private:
		AssetEditorRegistry m_Registry;
		AssetEditSession m_Session;
		std::unique_ptr<IAssetEditor> m_Editor;
	};
}
