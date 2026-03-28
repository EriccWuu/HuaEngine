#pragma once

#include <array>
#include <filesystem>

#include "HuaEngine.h"
#include "HuaEngine/Core/ResultEnvelope.h"
#include "Workbench/EditorSessionStorage.h"

namespace HE {
	class ProjectHubLayer : public Layer {
	public:
		ProjectHubLayer();

		void OnAttach() override;
		void OnGuiRender() override;

	private:
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
