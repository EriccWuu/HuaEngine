#pragma once

#include <filesystem>
#include <functional>
#include <optional>

#include "Assets/AssetInspectorHost.h"
#include "Assets/AssetPickerCatalog.h"
#include "HuaEngine/Project/ProjectContext.h"

namespace HE {
	class EditorWorkbenchState;
}

namespace HE::Editor {
	class AssetInspectorEditor {
	public:
		explicit AssetInspectorEditor(AssetPickerCatalog& pickerCatalog);
		~AssetInspectorEditor();

		bool Draw();
		void DrawModals();

		[[nodiscard]] bool HasDirtyEdit() const;
		[[nodiscard]] ResultEnvelope Apply(AssetApplyState* outState = nullptr);
		void Revert();
		bool RequestDirtyResolution(std::function<void()> continuation);
		void CheckExternalModification();

		void BindProject(const ProjectContext* projectContext);
		void SetWorkbenchState(EditorWorkbenchState* state) { m_WorkbenchState = state; }
		void SetOpenSceneCallback(std::function<void(const std::filesystem::path&)> callback) { m_OpenSceneCallback = std::move(callback); }

	private:
		void QueueReload(const AssetGuid& guid);
		void ProcessPendingReload();
		void RecordResult(const ResultEnvelope& result);

		AssetPickerCatalog& m_PickerCatalog;
		AssetInspectorHost m_Host;
		EditorWorkbenchState* m_WorkbenchState = nullptr;
		const ProjectContext* m_ProjectContext = nullptr;
		std::function<void()> m_DirtyContinuation;
		std::function<void(const std::filesystem::path&)> m_OpenSceneCallback;
		std::optional<AssetGuid> m_PendingReloadGuid;
		bool m_OpenDirtyPopup = false;
	};
}
