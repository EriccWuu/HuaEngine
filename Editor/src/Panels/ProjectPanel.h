#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>

#include "Assets/AssetCreationRegistry.h"
#include "HuaEngine/Asset/AssetRegistry.h"
#include "Workbench/EditorWorkbenchState.h"

namespace HE {
	namespace Editor { class EditorInputService; }
	enum class ProjectPanelActionType {
		None,
		OpenScene,
		OpenSource,
		RefreshProject,
		ReimportPath,
		ReimportAll,
		SelectAsset,
		CreateAsset,
		RenameAsset
	};

	struct ProjectPanelAction {
		ProjectPanelActionType Type = ProjectPanelActionType::None;
		std::filesystem::path Path;
		AssetGuid Guid;
		std::string TypeId;
		std::string Name;
	};

	[[nodiscard]] ProjectPanelAction MakeProjectReimportAction(
		const std::filesystem::path& targetPath,
		bool reimportAll);
	[[nodiscard]] ProjectPanelAction MakeProjectCreateAssetAction(
		std::string typeId,
		const std::filesystem::path& targetDirectory);
	[[nodiscard]] ProjectPanelAction MakeProjectRenameAssetAction(AssetGuid guid, std::string newBaseName);
	[[nodiscard]] bool IsProjectPanelVisibleFile(const std::filesystem::path& path);

	class ProjectPanel {
	public:
		void OnGuiRender();
		void SetWorkbenchState(const EditorWorkbenchState* state) { m_WorkbenchState = state; }
		void SetProjectRoot(const std::filesystem::path& rootPath) { m_ProjectRoot = rootPath; }
		void SetCurrentScenePath(const std::filesystem::path& scenePath) { m_CurrentScenePath = scenePath; }
		void SetAssetRecords(std::span<const AssetRecord> records);
		void SetCreationRegistry(const Editor::AssetCreationRegistry* registry) { m_CreationRegistry = registry; }
		void SetSelectedAssetGuid(AssetGuid guid) { m_SelectedAssetGuid = std::move(guid); }
		void BeginRename(const AssetGuid& guid);
		void CancelRename();
		void SetCanReimportCallback(std::function<bool(const std::filesystem::path&)> callback) { m_CanReimport = std::move(callback); }
		void SetInputService(Editor::EditorInputService* input) { m_Input = input; }
		[[nodiscard]] bool IsFocused() const { return m_IsFocused; }
		[[nodiscard]] bool IsHovered() const { return m_IsHovered; }
		[[nodiscard]] std::optional<ProjectPanelAction> ConsumePendingAction();

	private:
		void DrawDirectorySection(const char* label, const std::filesystem::path& rootPath);
		void DrawEntry(const std::filesystem::directory_entry& entry);
		void DrawCreateMenu(const std::filesystem::path& targetDirectory);
		void DrawFileEntry(const std::filesystem::directory_entry& entry, const AssetRecord* asset);

	private:
		const EditorWorkbenchState* m_WorkbenchState = nullptr;
		std::filesystem::path m_ProjectRoot;
		std::filesystem::path m_CurrentScenePath;
		std::function<bool(const std::filesystem::path&)> m_CanReimport;
		std::optional<ProjectPanelAction> m_PendingAction;
		std::unordered_map<std::string, AssetRecord> m_AssetsByPath;
		AssetGuid m_SelectedAssetGuid;
		AssetGuid m_RenamingAssetGuid;
		std::string m_RenameBuffer;
		bool m_RequestRenameFocus = false;
		const Editor::AssetCreationRegistry* m_CreationRegistry = nullptr;
		Editor::EditorInputService* m_Input = nullptr;
		bool m_IsFocused = false;
		bool m_IsHovered = false;
	};
}
