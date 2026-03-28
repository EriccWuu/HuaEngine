#pragma once

#include <filesystem>
#include <optional>

#include "Workbench/EditorWorkbenchState.h"

namespace HE {
	enum class ProjectPanelActionType {
		None,
		OpenScene,
		RefreshProject
	};

	struct ProjectPanelAction {
		ProjectPanelActionType Type = ProjectPanelActionType::None;
		std::filesystem::path Path;
	};

	class ProjectPanel {
	public:
		void OnGuiRender();
		void SetWorkbenchState(const EditorWorkbenchState* state) { m_WorkbenchState = state; }
		void SetProjectRoot(const std::filesystem::path& rootPath) { m_ProjectRoot = rootPath; }
		void SetCurrentScenePath(const std::filesystem::path& scenePath) { m_CurrentScenePath = scenePath; }
		[[nodiscard]] std::optional<ProjectPanelAction> ConsumePendingAction();

	private:
		void DrawDirectorySection(const char* label, const std::filesystem::path& rootPath);
		void DrawEntry(const std::filesystem::directory_entry& entry);

	private:
		const EditorWorkbenchState* m_WorkbenchState = nullptr;
		std::filesystem::path m_ProjectRoot;
		std::filesystem::path m_CurrentScenePath;
		std::optional<ProjectPanelAction> m_PendingAction;
	};
}
