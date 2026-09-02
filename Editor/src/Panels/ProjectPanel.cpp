#include "enginepch.h"
#include "ProjectPanel.h"

#include <algorithm>
#include <cctype>
#include <vector>

#include "imgui.h"
#include "Input/EditorInputService.h"

namespace {
	bool IsSceneFile(const std::filesystem::path& path) {
		return path.extension() == ".scene";
	}

	bool IsShaderFile(const std::filesystem::path& path) {
		auto extension = path.extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
		return extension == ".shader";
	}
}

namespace HE {
	ProjectPanelAction MakeProjectReimportAction(
		const std::filesystem::path& targetPath,
		bool reimportAll) {
		return {
			reimportAll ? ProjectPanelActionType::ReimportAll : ProjectPanelActionType::ReimportPath,
			reimportAll ? std::filesystem::path{} : targetPath
		};
	}

	ProjectPanelAction MakeProjectCreateAssetAction(
		std::string typeId,
		const std::filesystem::path& targetDirectory) {
		return {
			.Type = ProjectPanelActionType::CreateAsset,
			.Path = targetDirectory,
			.TypeId = std::move(typeId)
		};
	}

	ProjectPanelAction MakeProjectRenameAssetAction(AssetGuid guid, std::string newBaseName) {
		return {
			.Type = ProjectPanelActionType::RenameAsset,
			.Guid = std::move(guid),
			.Name = std::move(newBaseName)
		};
	}

	ProjectPanelAction MakeProjectDeleteAssetAction(AssetGuid guid) {
		return {
			.Type = ProjectPanelActionType::DeleteAsset,
			.Guid = std::move(guid)
		};
	}

	bool IsProjectPanelVisibleFile(const std::filesystem::path& path) {
		auto extension = path.extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
		return extension != ".meta";
	}

	void ProjectPanel::SetAssetRecords(std::span<const AssetRecord> records) {
		m_AssetsByPath.clear();
		for (const auto& record : records) {
			if (record.Source != AssetSource::File || record.AbsolutePath.empty()) continue;
			m_AssetsByPath[record.AbsolutePath.lexically_normal().generic_string()] = record;
		}
	}

	void ProjectPanel::BeginRename(const AssetGuid& guid) {
		m_RenamingAssetGuid = guid;
		m_RenameBuffer.clear();
		for (const auto& [path, record] : m_AssetsByPath) {
			if (record.Guid != guid) continue;
			m_RenameBuffer = std::filesystem::path(path).stem().string();
			break;
		}
		m_RequestRenameFocus = true;
		m_RenameSubmissionPending = false;
	}

	void ProjectPanel::RetryRename(const AssetGuid& guid, std::string_view draft) {
		m_RenamingAssetGuid = guid;
		m_RenameBuffer = draft;
		m_RequestRenameFocus = true;
		m_RenameSubmissionPending = false;
	}

	void ProjectPanel::CompleteRename(const AssetGuid& guid) {
		if (m_RenamingAssetGuid != guid) return;
		m_RenamingAssetGuid.clear();
		m_RenameBuffer.clear();
		m_RequestRenameFocus = false;
		m_RenameSubmissionPending = false;
		if (m_DeferredSelectionAction) {
			m_PendingAction = std::move(m_DeferredSelectionAction);
			m_DeferredSelectionAction.reset();
		}
	}

	void ProjectPanel::CancelRename() {
		m_RenamingAssetGuid.clear();
		m_RenameBuffer.clear();
		m_RequestRenameFocus = false;
		m_RenameSubmissionPending = false;
		m_DeferredSelectionAction.reset();
	}

	void ProjectPanel::QueueAssetSelection(AssetGuid guid, std::filesystem::path path) {
		ProjectPanelAction action{
			.Type = ProjectPanelActionType::SelectAsset,
			.Path = std::move(path),
			.Guid = std::move(guid)
		};
		if (IsRenaming()) {
			m_DeferredSelectionAction = std::move(action);
			return;
		}
		m_PendingAction = std::move(action);
	}

	std::optional<ProjectPanelAction> ProjectPanel::ConsumePendingAction() {
		auto action = m_PendingAction;
		m_PendingAction.reset();
		return action;
	}

	void ProjectPanel::OnGuiRender() {
		ImGui::Begin("Project");
		m_IsFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		m_IsHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

		if (m_WorkbenchState) {
			if (const auto* session = m_WorkbenchState->GetProjectSessionSummary()) {
				ImGui::Text("Project: %s", session->ProjectName.c_str());
				ImGui::Text("Status: %s", session->Operational ? "Operational" : "Needs attention");
				ImGui::TextWrapped("Root: %s", session->RootPath.c_str());
			} else {
				ImGui::TextUnformatted("No active project session.");
			}

			if (const auto* scene = m_WorkbenchState->GetSceneDocumentSummary()) {
				ImGui::Separator();
				ImGui::Text("Scene: %s", scene->DisplayName.c_str());
				ImGui::Text("Dirty: %s", scene->Dirty ? "Yes" : "No");
			}

			if (const auto* validation = m_WorkbenchState->GetLastValidationReport()) {
				ImGui::Separator();
				ImGui::Text("Validation: %u domains", validation->DomainCount);
				ImGui::SameLine();
				ImGui::Text("W:%u E:%u", validation->WarningCount, validation->ErrorCount);
			}
		}

		ImGui::Separator();
		if (ImGui::Button("Refresh Project")) {
			m_PendingAction = ProjectPanelAction{ ProjectPanelActionType::RefreshProject, {} };
		}

		if (m_ProjectRoot.empty()) {
			ImGui::Spacing();
			ImGui::TextUnformatted("Open or create a project to browse its workspace.");
			ImGui::End();
			return;
		}

		DrawDirectorySection("Assets", m_ProjectRoot / "Assets");
		DrawDeleteConfirmation();

		ImGui::End();
	}

	void ProjectPanel::DrawDeleteConfirmation() {
		if (m_OpenDeleteConfirmation) {
			ImGui::OpenPopup("Delete Asset");
			m_OpenDeleteConfirmation = false;
		}
		if (!ImGui::BeginPopupModal("Delete Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;

		ImGui::TextWrapped("Delete '%s' from the project?", m_DeleteConfirmationName.c_str());
		ImGui::TextDisabled("The source file, metadata, and imported artifacts will be removed.");
		ImGui::Spacing();
		if (ImGui::Button("Delete")) {
			m_PendingAction = MakeProjectDeleteAssetAction(m_DeleteConfirmationGuid);
			m_DeleteConfirmationGuid.clear();
			m_DeleteConfirmationName.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) {
			m_DeleteConfirmationGuid.clear();
			m_DeleteConfirmationName.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	void ProjectPanel::DrawDirectorySection(const char* label, const std::filesystem::path& rootPath) {
		const auto rootId = rootPath.generic_string();
		ImGui::PushID(rootId.c_str());
		const bool open = ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen);
		if (ImGui::BeginPopupContextItem("AssetsHeaderContext")) {
			DrawCreateMenu(rootPath);
			if (ImGui::MenuItem("Reimport All")) {
				m_PendingAction = MakeProjectReimportAction({}, true);
			}
			ImGui::EndPopup();
		}
		ImGui::PopID();
		if (!open) {
			return;
		}

		if (!std::filesystem::exists(rootPath)) {
			ImGui::TextDisabled("%s is missing", rootPath.filename().string().c_str());
			return;
		}

		ImGui::BeginChild("AssetsBrowser", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
		std::vector<std::filesystem::directory_entry> entries;
		for (const auto& entry : std::filesystem::directory_iterator(rootPath)) {
			if (!entry.is_directory() && !IsProjectPanelVisibleFile(entry.path())) continue;
			entries.push_back(entry);
		}

		std::sort(entries.begin(), entries.end(), [](const auto& lhs, const auto& rhs) {
			if (lhs.is_directory() != rhs.is_directory()) {
				return lhs.is_directory() > rhs.is_directory();
			}

			return lhs.path().filename().string() < rhs.path().filename().string();
		});

		for (const auto& entry : entries) {
			DrawEntry(entry);
		}

		if (ImGui::BeginPopupContextWindow(
			"AssetsBrowserContext",
			ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
			DrawCreateMenu(rootPath);
			if (ImGui::MenuItem("Reimport All")) {
				m_PendingAction = MakeProjectReimportAction({}, true);
			}
			ImGui::EndPopup();
		}
		ImGui::EndChild();
	}

	void ProjectPanel::DrawCreateMenu(const std::filesystem::path& targetDirectory) {
		if (!m_CreationRegistry || !ImGui::BeginMenu("Create")) return;
		for (const auto& descriptor : m_CreationRegistry->GetAll()) {
			if (ImGui::MenuItem(descriptor.MenuLabel.c_str())) {
				m_PendingAction = MakeProjectCreateAssetAction(descriptor.TypeId, targetDirectory);
			}
		}
		ImGui::EndMenu();
	}

	void ProjectPanel::DrawEntry(const std::filesystem::directory_entry& entry) {
		const auto fileName = entry.path().filename().string();
		const auto entryId = entry.path().lexically_normal().generic_string();
		ImGui::PushID(entryId.c_str());
		if (entry.is_directory()) {
			const bool open = ImGui::TreeNode(fileName.c_str());
			if (ImGui::BeginPopupContextItem("DirectoryContext")) {
				DrawCreateMenu(entry.path());
				if (ImGui::MenuItem("Reimport")) {
					m_PendingAction = MakeProjectReimportAction(entry.path(), false);
				}
				ImGui::EndPopup();
			}
			if (open) {
				std::vector<std::filesystem::directory_entry> children;
				for (const auto& child : std::filesystem::directory_iterator(entry.path())) {
					if (!child.is_directory() && !IsProjectPanelVisibleFile(child.path())) continue;
					children.push_back(child);
				}

				std::sort(children.begin(), children.end(), [](const auto& lhs, const auto& rhs) {
					if (lhs.is_directory() != rhs.is_directory()) {
						return lhs.is_directory() > rhs.is_directory();
					}

					return lhs.path().filename().string() < rhs.path().filename().string();
				});

				for (const auto& child : children) {
					DrawEntry(child);
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
			return;
		}

		const auto asset = m_AssetsByPath.find(entry.path().lexically_normal().generic_string());
		DrawFileEntry(entry, asset != m_AssetsByPath.end() ? &asset->second : nullptr);
		ImGui::PopID();
	}

	void ProjectPanel::DrawFileEntry(const std::filesystem::directory_entry& entry, const AssetRecord* asset) {
		const auto fileName = entry.path().filename().string();
		const bool renaming = asset && asset->Guid == m_RenamingAssetGuid;
		if (renaming) {
			if (m_RequestRenameFocus) {
				ImGui::SetKeyboardFocusHere();
				m_RequestRenameFocus = false;
			}

			m_RenameBuffer.resize(256, '\0');
			ImGui::BeginDisabled(m_RenameSubmissionPending);
			const bool submitted = ImGui::InputText(
				"##Rename",
				m_RenameBuffer.data(),
				m_RenameBuffer.size(),
				ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
			const bool commitAfterFocusLoss = ImGui::IsItemDeactivated();
			ImGui::EndDisabled();
			m_RenameBuffer.resize(std::char_traits<char>::length(m_RenameBuffer.c_str()));
			if (!m_RenameSubmissionPending && (submitted || commitAfterFocusLoss)) {
				m_PendingAction = MakeProjectRenameAssetAction(asset->Guid, m_RenameBuffer);
				m_RenameSubmissionPending = true;
			}
		}
		else {
			const bool selected = asset && asset->Guid == m_SelectedAssetGuid;
			if (ImGui::Selectable(fileName.c_str(), selected) && asset) {
				QueueAssetSelection(asset->Guid, entry.path());
			}
		}

		if (ImGui::IsItemHovered() && m_Input && m_Input->WasActionTriggered("editor.project.open_item") && IsSceneFile(entry.path())) {
			m_PendingAction = ProjectPanelAction{ ProjectPanelActionType::OpenScene, entry.path() };
		}
		if (ImGui::IsItemHovered() && m_Input && m_Input->WasActionTriggered("editor.project.open_item") && IsShaderFile(entry.path())) {
			m_PendingAction = ProjectPanelAction{ ProjectPanelActionType::OpenSource, entry.path() };
		}

		if (ImGui::BeginPopupContextItem("FileContext")) {
			ImGui::BeginDisabled(!asset);
			if (ImGui::MenuItem("Rename") && asset) {
				BeginRename(asset->Guid);
			}
			if (ImGui::MenuItem("Delete") && asset) {
				m_DeleteConfirmationGuid = asset->Guid;
				m_DeleteConfirmationName = fileName;
				m_OpenDeleteConfirmation = true;
			}
			ImGui::EndDisabled();
			const bool canReimport = m_CanReimport && m_CanReimport(entry.path());
			ImGui::BeginDisabled(!canReimport);
			if (ImGui::MenuItem("Reimport")) {
				m_PendingAction = MakeProjectReimportAction(entry.path(), false);
			}
			ImGui::EndDisabled();
			if (!canReimport) {
				ImGui::SetItemTooltip("No importer supports this file type.");
			}
			ImGui::EndPopup();
		}
	}
}
