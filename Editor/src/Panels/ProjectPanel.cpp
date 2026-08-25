#include "enginepch.h"
#include "ProjectPanel.h"

#include <algorithm>
#include <cctype>
#include <vector>

#include "imgui.h"

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

	void ProjectPanel::SetAssetRecords(std::span<const AssetRecord> records) {
		m_AssetsByPath.clear();
		for (const auto& record : records) {
			if (record.Source != AssetSource::File || record.AbsolutePath.empty()) continue;
			m_AssetsByPath[record.AbsolutePath.lexically_normal().generic_string()] = record;
		}
	}

	std::optional<ProjectPanelAction> ProjectPanel::ConsumePendingAction() {
		auto action = m_PendingAction;
		m_PendingAction.reset();
		return action;
	}

	void ProjectPanel::OnGuiRender() {
		ImGui::Begin("Project");

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

		ImGui::End();
	}

	void ProjectPanel::DrawDirectorySection(const char* label, const std::filesystem::path& rootPath) {
		const auto rootId = rootPath.generic_string();
		ImGui::PushID(rootId.c_str());
		const bool open = ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen);
		if (ImGui::BeginPopupContextItem("AssetsHeaderContext")) {
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
			if (ImGui::MenuItem("Reimport All")) {
				m_PendingAction = MakeProjectReimportAction({}, true);
			}
			ImGui::EndPopup();
		}
		ImGui::EndChild();
	}

	void ProjectPanel::DrawEntry(const std::filesystem::directory_entry& entry) {
		const auto fileName = entry.path().filename().string();
		const auto entryId = entry.path().lexically_normal().generic_string();
		ImGui::PushID(entryId.c_str());
		if (entry.is_directory()) {
			const bool open = ImGui::TreeNode(fileName.c_str());
			if (ImGui::BeginPopupContextItem("DirectoryContext")) {
				if (ImGui::MenuItem("Reimport")) {
					m_PendingAction = MakeProjectReimportAction(entry.path(), false);
				}
				ImGui::EndPopup();
			}
			if (open) {
				std::vector<std::filesystem::directory_entry> children;
				for (const auto& child : std::filesystem::directory_iterator(entry.path())) {
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
		const bool selected = asset != m_AssetsByPath.end() && asset->second.Guid == m_SelectedAssetGuid;
		if (ImGui::Selectable(fileName.c_str(), selected) && asset != m_AssetsByPath.end()) {
			m_PendingAction = ProjectPanelAction{ .Type = ProjectPanelActionType::SelectAsset, .Path = entry.path(), .Guid = asset->second.Guid };
		}

		if (ImGui::IsItemHovered() && IsSceneFile(entry.path()) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
			m_PendingAction = ProjectPanelAction{ ProjectPanelActionType::OpenScene, entry.path() };
		}
		if (ImGui::IsItemHovered() && IsShaderFile(entry.path()) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
			m_PendingAction = ProjectPanelAction{ ProjectPanelActionType::OpenSource, entry.path() };
		}

		if (ImGui::BeginPopupContextItem("FileContext")) {
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
		ImGui::PopID();
	}
}
