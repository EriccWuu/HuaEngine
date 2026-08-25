#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <unordered_map>

#include "HuaEngine/Asset/AssetRegistry.h"
#include "Workbench/EditorWorkbenchState.h"

namespace HE {
	enum class ProjectPanelActionType {
		None,
		OpenScene,
		OpenSource,
		RefreshProject,
		ReimportPath,
		ReimportAll,
		SelectAsset
	};

	struct ProjectPanelAction {
		ProjectPanelActionType Type = ProjectPanelActionType::None;
		std::filesystem::path Path;
		AssetGuid Guid;
	};

	[[nodiscard]] ProjectPanelAction MakeProjectReimportAction(
		const std::filesystem::path& targetPath,
		bool reimportAll);
	[[nodiscard]] bool IsProjectPanelVisibleFile(const std::filesystem::path& path);

	class ProjectPanel {
	public:
		void OnGuiRender();
		void SetWorkbenchState(const EditorWorkbenchState* state) { m_WorkbenchState = state; }
		void SetProjectRoot(const std::filesystem::path& rootPath) { m_ProjectRoot = rootPath; }
		void SetCurrentScenePath(const std::filesystem::path& scenePath) { m_CurrentScenePath = scenePath; }
		void SetAssetRecords(std::span<const AssetRecord> records);
		void SetSelectedAssetGuid(AssetGuid guid) { m_SelectedAssetGuid = std::move(guid); }
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
		std::unordered_map<std::string, AssetRecord> m_AssetsByPath;
		AssetGuid m_SelectedAssetGuid;
	};
}
