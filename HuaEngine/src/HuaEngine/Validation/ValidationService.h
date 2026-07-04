#pragma once

#include <cstdint>

#include "HuaEngine/Asset/AssetService.h"
#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Core/ResultEnvelope.h"
#include "HuaEngine/Project/ProjectService.h"
#include "HuaEngine/Scene/SceneService.h"

namespace HE {
	class ProjectService;
	class SceneService;

	struct ValidationRequest {
		const ProjectContext* Project = nullptr;
		const Scene* SceneTarget = nullptr;
		const AssetService* Assets = nullptr;
	};

	struct ValidationReport {
		uint32_t DomainCount = 0;
		uint32_t SuccessCount = 0;
		uint32_t FailureCount = 0;
		uint32_t ManualInterventionCount = 0;
		uint32_t WarningCount = 0;
		uint32_t ErrorCount = 0;

		bool IncludesProject = false;
		bool IncludesScene = false;
		bool IncludesAssets = false;

		ProjectStatusReport ProjectStatus;
		SceneValidationReport SceneStatus;
		AssetValidationReport AssetStatus;

		ResultEnvelope ProjectResult;
		ResultEnvelope SceneResult;
		ResultEnvelope AssetResult;

		[[nodiscard]] bool IsOperational() const {
			return FailureCount == 0 && ManualInterventionCount == 0;
		}

		[[nodiscard]] bool HasBlockingFailures() const {
			return FailureCount > 0;
		}

		[[nodiscard]] bool RequiresManualIntervention() const {
			return ManualInterventionCount > 0;
		}
	};

	class ENGINE_API ValidationService {
	public:
		ValidationService(ProjectService& projectService, SceneService& sceneService)
			: m_ProjectService(&projectService), m_SceneService(&sceneService) {}

		[[nodiscard]] ResultEnvelope Validate(const ValidationRequest& request, ValidationReport* outReport = nullptr) const;

	private:
		ProjectService* m_ProjectService = nullptr;
		SceneService* m_SceneService = nullptr;
	};
}
