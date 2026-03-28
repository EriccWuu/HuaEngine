#include "enginepch.h"
#include "ProjectHubLayer.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string_view>

#include "HuaEngine/Application/ApplicationOperations.h"
#include "HuaEngine/Core/HostLaunch.h"
#include "imgui.h"

namespace {
	void CopyToBuffer(std::string_view value, char* buffer, size_t size) {
		if (!buffer || size == 0) {
			return;
		}

		const auto copyLength = (std::min)(value.size(), size - 1);
		std::memcpy(buffer, value.data(), copyLength);
		buffer[copyLength] = '\0';
	}

	std::filesystem::path ResolveDefaultProjectRoot() {
		const char* localAppData = std::getenv("LOCALAPPDATA");
		if (localAppData && *localAppData) {
			return std::filesystem::path(localAppData) / "HuaEngine" / "Projects" / "MyProject";
		}

		return std::filesystem::temp_directory_path() / "huaengine_projects" / "MyProject";
	}

	std::filesystem::path NormalizePath(const std::filesystem::path& path) {
		if (path.empty()) {
			return {};
		}

		std::error_code errorCode;
		auto absolutePath = std::filesystem::absolute(path, errorCode);
		if (errorCode) {
			return path.lexically_normal();
		}

		if (std::filesystem::exists(absolutePath, errorCode)) {
			auto canonicalPath = std::filesystem::weakly_canonical(absolutePath, errorCode);
			if (!errorCode) {
				return canonicalPath;
			}
		}

		return absolutePath.lexically_normal();
	}
}

namespace HE {
	ProjectHubLayer::ProjectHubLayer()
		: Layer("ProjectHubLayer") {
	}

	void ProjectHubLayer::OnAttach() {
		CopyToBuffer(ResolveDefaultProjectRoot().generic_string(), m_ProjectPathInput.data(), m_ProjectPathInput.size());
		CopyToBuffer("MyProject", m_ProjectNameInput.data(), m_ProjectNameInput.size());
		RefreshSession();
	}

	void ProjectHubLayer::OnGuiRender() {
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 16.0f));

		constexpr ImGuiWindowFlags windowFlags =
			ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoBringToFrontOnFocus |
			ImGuiWindowFlags_NoNavFocus;

		ImGui::Begin("Project Hub", nullptr, windowFlags);
		ImGui::PopStyleVar(3);

		ImGui::TextUnformatted("HuaEngine Project Hub");
		ImGui::Separator();
		ImGui::TextWrapped("Create a new project, open an existing project, or resume the last active one. ProjectHub is now the formal no-project entry host.");
		ImGui::Spacing();

		const ImVec2 tableSize = ImGui::GetContentRegionAvail();
		if (ImGui::BeginTable("Project Hub Layout", 2,
			ImGuiTableFlags_SizingStretchProp |
			ImGuiTableFlags_BordersInnerV |
			ImGuiTableFlags_Resizable,
			tableSize)) {
			ImGui::TableSetupColumn("Session", ImGuiTableColumnFlags_WidthStretch, 0.95f);
			ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthStretch, 1.35f);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::BeginChild("Project Hub Session", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_None);
			if (m_HasPersistedSession) {
				ImGui::SeparatorText("Resume");
				ImGui::Text("Recent Project: %s", m_PersistedSession.LastProjectName.empty() ? "<unnamed>" : m_PersistedSession.LastProjectName.c_str());
				ImGui::TextWrapped("Root: %s", m_PersistedSession.LastProjectRoot.c_str());
				if (!m_PersistedSession.LastScenePath.empty()) {
					ImGui::TextWrapped("Last Scene: %s", m_PersistedSession.LastScenePath.c_str());
				}

				if (ImGui::Button("Resume Last Project", ImVec2(-1.0f, 0.0f))) {
					ResumeLastProject();
				}
				if (ImGui::Button("Reset Saved Session", ImVec2(-1.0f, 0.0f))) {
					EditorSessionStorage::Clear();
					RefreshSession();
				}
			} else {
				ImGui::SeparatorText("Resume");
				ImGui::TextWrapped("No persisted project session is available yet. Create or open a project to seed the launcher session state.");
			}

			ImGui::Spacing();
			ImGui::SeparatorText("Status");
			if (!m_LastResult.Operation.empty()) {
				ImGui::TextWrapped("%s", m_LastResult.Summary.c_str());
				ImGui::TextDisabled("Operation: %s", m_LastResult.Operation.c_str());
			} else {
				ImGui::TextUnformatted("Ready.");
			}
			ImGui::EndChild();

			ImGui::TableSetColumnIndex(1);
			ImGui::BeginChild("Project Hub Actions", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_None);
			ImGui::SeparatorText("Project");
			ImGui::TextUnformatted("Project Root");
			ImGui::InputText("##ProjectRoot", m_ProjectPathInput.data(), static_cast<int>(m_ProjectPathInput.size()));
			ImGui::TextUnformatted("Project Name");
			ImGui::InputText("##ProjectName", m_ProjectNameInput.data(), static_cast<int>(m_ProjectNameInput.size()));

			if (ImGui::Button("Create Project", ImVec2(-1.0f, 0.0f))) {
				CreateProjectAndLaunch();
			}
			if (ImGui::Button("Open Project", ImVec2(-1.0f, 0.0f))) {
				OpenProjectAndLaunch();
			}
			ImGui::EndChild();

			ImGui::EndTable();
		}
		ImGui::End();
	}

	bool ProjectHubLayer::CreateProjectAndLaunch() {
		const auto projectRoot = NormalizePath(m_ProjectPathInput.data());
		ProjectContext context;
		auto result = Application::GetInstance().GetOperations().InitializeProject(projectRoot, &context, m_ProjectNameInput.data());
		CaptureResult(result);
		if (!result.Succeeded()) {
			return false;
		}

		return LaunchEditor(context.RootPath);
	}

	bool ProjectHubLayer::OpenProjectAndLaunch() {
		const auto projectRoot = NormalizePath(m_ProjectPathInput.data());
		ProjectContext context;
		auto resolveResult = Application::GetInstance().GetOperations().ResolveProjectContext(projectRoot, context);
		CaptureResult(resolveResult);
		if (!resolveResult.Succeeded()) {
			return false;
		}

		ProjectStatusReport status;
		auto statusResult = Application::GetInstance().GetOperations().CheckProjectStatus(context, &status);
		CaptureResult(statusResult);
		if (!statusResult.Succeeded()) {
			return false;
		}

		return LaunchEditor(context.RootPath);
	}

	bool ProjectHubLayer::ResumeLastProject() {
		if (!m_HasPersistedSession || m_PersistedSession.LastProjectRoot.empty()) {
			auto result = ResultEnvelope::Failure("project_hub.resume", "project_hub", "No persisted project session is available");
			CaptureResult(result);
			return false;
		}

		std::filesystem::path scenePath;
		if (!m_PersistedSession.LastScenePath.empty()) {
			const auto candidate = NormalizePath(m_PersistedSession.LastScenePath);
			std::error_code errorCode;
			if (std::filesystem::exists(candidate, errorCode) && std::filesystem::is_regular_file(candidate, errorCode)) {
				scenePath = candidate;
			}
		}

		return LaunchEditor(NormalizePath(m_PersistedSession.LastProjectRoot), scenePath);
	}

	bool ProjectHubLayer::LaunchEditor(const std::filesystem::path& projectRoot, const std::filesystem::path& scenePath) {
		std::vector<std::string> arguments = { "--project", projectRoot.generic_string() };
		if (!scenePath.empty()) {
			arguments.emplace_back("--scene");
			arguments.emplace_back(scenePath.generic_string());
		}

		if (!HostLaunch::LaunchSibling("Editor.exe", arguments, projectRoot)) {
			auto result = ResultEnvelope::Failure("project_hub.launch_editor", "Editor.exe", "Failed to launch Editor host");
			result.AddDetail({ DiagnosticSeverity::Error, "project_hub.launch_editor.failed", "Editor executable could not be launched from the current output directory", HostLaunch::ResolveSiblingExecutable("Editor.exe").generic_string() });
			CaptureResult(result);
			return false;
		}

		auto result = ResultEnvelope::Success("project_hub.launch_editor", projectRoot.generic_string(), "Editor launched successfully");
		if (!scenePath.empty()) {
			result.SetPayloadValue("scene_path", scenePath.generic_string());
		}
		CaptureResult(result);
		Application::GetInstance().RequestShutdown();
		return true;
	}

	void ProjectHubLayer::RefreshSession() {
		m_HasPersistedSession = EditorSessionStorage::Load(m_PersistedSession);
		if (m_HasPersistedSession && !m_PersistedSession.LastProjectRoot.empty()) {
			CopyToBuffer(m_PersistedSession.LastProjectRoot, m_ProjectPathInput.data(), m_ProjectPathInput.size());
			if (!m_PersistedSession.LastProjectName.empty()) {
				CopyToBuffer(m_PersistedSession.LastProjectName, m_ProjectNameInput.data(), m_ProjectNameInput.size());
			}
		}
	}

	void ProjectHubLayer::CaptureResult(const ResultEnvelope& result) {
		m_LastResult = result;
		if (result.Succeeded()) {
			HE_CORE_INFO("[ProjectHub] {}", result.Summary);
			return;
		}

		HE_CORE_ERROR("[ProjectHub] {} ({})", result.Summary, result.Operation);
		for (const auto& detail : result.Details) {
			HE_CORE_ERROR("[ProjectHub] {} :: {}", detail.Code, detail.Message);
		}
	}
}
