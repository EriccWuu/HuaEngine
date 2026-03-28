#pragma once

#include <optional>
#include <string>
#include <vector>

#include "HuaEngine/Core/ResultEnvelope.h"
#include "HuaEngine/Validation/ValidationService.h"

namespace HE {
	struct ProjectSessionSummary {
		bool Loaded = false;
		bool Operational = false;
		std::string ProjectName;
		std::string RootPath;
		std::string ProjectFilePath;
		std::string LastOpenedScenePath;
	};

	struct SceneDocumentSummary {
		bool Loaded = false;
		bool Dirty = false;
		std::string DisplayName;
		std::string ScenePath;
		std::string Source;
		size_t EntityCount = 0;
	};

	struct WorkbenchEvent {
		ResultEnvelope Result;
		std::string Source;
	};

	class EditorWorkbenchState {
	public:
		void SetProjectSessionSummary(const ProjectSessionSummary& summary) {
			m_ProjectSession = summary;
		}

		void ClearProjectSessionSummary() {
			m_ProjectSession.reset();
		}

		void SetSceneDocumentSummary(const SceneDocumentSummary& summary) {
			m_SceneDocument = summary;
		}

		void ClearSceneDocumentSummary() {
			m_SceneDocument.reset();
		}

		void RecordEvent(const ResultEnvelope& result, std::string source = {}) {
			m_LastResult = result;
			m_LastSource = std::move(source);
			m_EventHistory.push_back({ result, m_LastSource });
			if (m_EventHistory.size() > 16) {
				m_EventHistory.erase(m_EventHistory.begin());
			}
		}

		void CaptureResult(const ResultEnvelope& result, std::string source = {}) {
			RecordEvent(result, std::move(source));
		}

		void CaptureValidation(const ResultEnvelope& result, const ValidationReport& report, std::string source = {}) {
			CaptureResult(result, std::move(source));
			m_LastValidationResult = result;
			m_LastValidationReport = report;
		}

		[[nodiscard]] const ResultEnvelope* GetLastResult() const {
			return m_LastResult.Operation.empty() ? nullptr : &m_LastResult;
		}

		[[nodiscard]] const ResultEnvelope* GetLastValidationResult() const {
			return m_LastValidationResult ? &*m_LastValidationResult : nullptr;
		}

		[[nodiscard]] const ValidationReport* GetLastValidationReport() const {
			return m_LastValidationReport ? &*m_LastValidationReport : nullptr;
		}

		[[nodiscard]] const std::string& GetLastSource() const {
			return m_LastSource;
		}

		[[nodiscard]] const std::vector<WorkbenchEvent>& GetEventHistory() const {
			return m_EventHistory;
		}

		[[nodiscard]] const ProjectSessionSummary* GetProjectSessionSummary() const {
			return m_ProjectSession ? &*m_ProjectSession : nullptr;
		}

		[[nodiscard]] const SceneDocumentSummary* GetSceneDocumentSummary() const {
			return m_SceneDocument ? &*m_SceneDocument : nullptr;
		}

	private:
		ResultEnvelope m_LastResult;
		std::optional<ResultEnvelope> m_LastValidationResult;
		std::optional<ValidationReport> m_LastValidationReport;
		std::optional<ProjectSessionSummary> m_ProjectSession;
		std::optional<SceneDocumentSummary> m_SceneDocument;
		std::string m_LastSource;
		std::vector<WorkbenchEvent> m_EventHistory;
	};
}
