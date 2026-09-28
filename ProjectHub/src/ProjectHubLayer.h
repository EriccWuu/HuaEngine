#pragma once

#include <array>
#include <filesystem>

#include "HuaEngine.h"
#include "HuaEngine/Core/ResultEnvelope.h"
#include "Workbench/EditorSessionStorage.h"
#include "Workbench/EcsHostSmoke.h"

namespace HE {
	class ProjectHubLayer : public Layer {
	public:
		explicit ProjectHubLayer(std::shared_ptr<HostSmoke::Session> smoke = {});

		void OnAttach() override;
		void OnUpdate() override;
		void OnEvent(Event& event) override;
		void OnGuiRender() override;

	private:
		std::shared_ptr<HostSmoke::Session> m_Smoke;
		HostSmoke::ChildProcess m_SmokeChild;
		std::filesystem::path m_SmokeProject;
		bool CreateProjectAndLaunch();
		bool OpenProjectAndLaunch();
		bool ResumeLastProject();
		bool LaunchEditor(const std::filesystem::path& projectRoot, const std::filesystem::path& scenePath = {});
		void RefreshSession();
		void CaptureResult(const ResultEnvelope& result);

	private:
		ResultEnvelope m_LastResult;
		PersistedEditorSession m_PersistedSession;
		bool m_HasPersistedSession = false;
		std::array<char, 512> m_ProjectPathInput{};
		std::array<char, 128> m_ProjectNameInput{};
	};
}
