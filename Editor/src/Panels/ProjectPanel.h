#pragma once

#include <filesystem>
#include <functional>
#include <optional>

#include "Workbench/EditorWorkbenchState.h"

namespace HE {
	enum class ProjectPanelActionType {
		None,
		OpenScene,
		RefreshProject,
		ReimportPath,
		ReimportAll
	};

	struct ProjectPanelAction {
		ProjectPanelActionType Type = ProjectPanelActionType::None;
		std::filesystem::path Path;
	};

	[[nodiscard]] ProjectPanelAction MakeProjectReimportAction(
		const std::filesystem::path& targetPath,
		bool reimportAll);

	class ProjectPanel {
	public:
		void OnGuiRender();
		void SetWorkbenchState(const EditorWorkbenchState* state) { m_WorkbenchState = state; }
		void SetProjectRoot(const std::filesystem::path& rootPath) { m_ProjectRoot = rootPath; }
		void SetCurrentScenePath(const std::filesystem::path& scenePath) { m_CurrentScenePath = scenePath; }
		void SetCanReimportCallback(std::function<bool(const std::filesystem::path&)> callback) { m_CanReimport = std::move(callback); }
		[[nodiscard]] std::optional<ProjectPanelAction> ConsumePendingAction();

	private:
		void DrawDirectorySection(const char* label, const std::filesystem::path& rootPath);
		void DrawEntry(const std::filesystem::directory_entry& entry);

	private:
		const EditorWorkbenchState* m_WorkbenchState = nullptr;
		std::filesystem::path m_ProjectRoot;
		std::filesystem::path m_CurrentScenePath;
		std::function<bool(const std::filesystem::path&)> m_CanReimport;
		std::optional<ProjectPanelAction> m_PendingAction;
	};
}
