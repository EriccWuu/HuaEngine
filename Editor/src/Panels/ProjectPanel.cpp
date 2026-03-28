#include "enginepch.h"
#include "ProjectPanel.h"

#include <algorithm>
#include <vector>

#include "imgui.h"

namespace {
	bool IsSceneFile(const std::filesystem::path& path) {
		return path.extension() == ".scene";
	}
}

namespace HE {
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
		DrawDirectorySection("Scenes", m_ProjectRoot / "Scenes");

		ImGui::End();
	}

	void ProjectPanel::DrawDirectorySection(const char* label, const std::filesystem::path& rootPath) {
		if (!ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen)) {
			return;
		}

		if (!std::filesystem::exists(rootPath)) {
			ImGui::TextDisabled("%s is missing", rootPath.filename().string().c_str());
			return;
		}

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
	}

	void ProjectPanel::DrawEntry(const std::filesystem::directory_entry& entry) {
		const auto fileName = entry.path().filename().string();
		if (entry.is_directory()) {
			const bool open = ImGui::TreeNode(fileName.c_str());
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
			return;
		}

		std::error_code errorCode;
		const bool selected = !m_CurrentScenePath.empty() && std::filesystem::equivalent(entry.path(), m_CurrentScenePath, errorCode);
		if (ImGui::Selectable(fileName.c_str(), selected) && IsSceneFile(entry.path())) {
			m_PendingAction = ProjectPanelAction{ ProjectPanelActionType::OpenScene, entry.path() };
		}

		if (ImGui::IsItemHovered() && IsSceneFile(entry.path()) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
			m_PendingAction = ProjectPanelAction{ ProjectPanelActionType::OpenScene, entry.path() };
		}
	}
}
