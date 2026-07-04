#include "enginepch.h"
#include "ValidationService.h"

namespace {
	std::string CountToString(uint32_t value) {
		return std::to_string(value);
	}

	std::string MakeValidationTarget(const HE::ValidationRequest& request) {
		if (request.Project && request.Project->IsLoaded()) {
			return request.Project->GetTargetId();
		}
		if (request.SceneTarget && !request.SceneTarget->GetName().empty()) {
			return request.SceneTarget->GetName();
		}

		return "<validation-scope>";
	}

	void AppendChildDiagnostics(HE::ResultEnvelope& aggregate, std::string_view domain, const HE::ResultEnvelope& child) {
		for (const auto& detail : child.Details) {
			auto merged = detail;
			if (merged.Context.empty()) {
				merged.Context = std::string(domain);
			}
			else {
				merged.Context = std::string(domain) + ": " + merged.Context;
			}

			aggregate.AddDetail(std::move(merged));
		}
	}

	void AccumulateStatus(HE::ValidationReport& report, const HE::ResultEnvelope& child) {
		++report.DomainCount;
		if (child.Succeeded()) {
			++report.SuccessCount;
		}
		else if (child.RequiresManualIntervention()) {
			++report.ManualInterventionCount;
		}
		else {
			++report.FailureCount;
		}

		for (const auto& detail : child.Details) {
			switch (detail.Severity) {
			case HE::DiagnosticSeverity::Warning:
				++report.WarningCount;
				break;
			case HE::DiagnosticSeverity::Error:
				++report.ErrorCount;
				break;
			case HE::DiagnosticSeverity::Info:
			default:
				break;
			}
		}
	}
}

	namespace HE {
	ResultEnvelope ValidationService::Validate(const ValidationRequest& request, ValidationReport* outReport) const {
		ValidationReport report;
		HE_CORE_ASSERT(m_ProjectService, "ValidationService requires a valid ProjectService");
		HE_CORE_ASSERT(m_SceneService, "ValidationService requires a valid SceneService");
		if (!request.Project && !request.SceneTarget && !request.Assets) {
			auto result = ResultEnvelope::Failure("validation.run", MakeValidationTarget(request), "Validation request is empty");
			result.AddDetail({ DiagnosticSeverity::Error, "validation.request.empty", "Validation requires at least one domain target", {} });
			if (outReport) {
				*outReport = report;
			}
			return result;
		}

		if (request.Project) {
			report.IncludesProject = true;
			report.ProjectResult = m_ProjectService->CheckProjectStatus(*request.Project, &report.ProjectStatus);
			AccumulateStatus(report, report.ProjectResult);
		}

		if (request.SceneTarget) {
			report.IncludesScene = true;
			report.SceneResult = m_SceneService->ValidateScene(*request.SceneTarget, &report.SceneStatus);
			AccumulateStatus(report, report.SceneResult);
		}

		if (request.Assets) {
			report.IncludesAssets = true;
			if (!request.Project) {
				report.AssetResult = ResultEnvelope::Failure("asset.validate", MakeValidationTarget(request), "Asset validation requires project context");
				report.AssetResult.AddDetail({ DiagnosticSeverity::Error, "validation.assets.project_missing", "Asset validation requires a loaded project context in the validation request", {} });
			}
			else {
				report.AssetResult = request.Assets->ValidateRegistry(*request.Project, &report.AssetStatus);
			}
			AccumulateStatus(report, report.AssetResult);
		}

		auto result = report.FailureCount > 0
			? ResultEnvelope::Failure("validation.run", MakeValidationTarget(request), "Validation detected blocking failures")
			: (report.ManualInterventionCount > 0
				? ResultEnvelope::ManualIntervention("validation.run", MakeValidationTarget(request), "Validation requires manual intervention")
				: ResultEnvelope::Success("validation.run", MakeValidationTarget(request), "Validation passed"));

		result.SetPayloadValue("validated_domain_count", CountToString(report.DomainCount));
		result.SetPayloadValue("success_domain_count", CountToString(report.SuccessCount));
		result.SetPayloadValue("failure_domain_count", CountToString(report.FailureCount));
		result.SetPayloadValue("manual_intervention_domain_count", CountToString(report.ManualInterventionCount));
		result.SetPayloadValue("warning_count", CountToString(report.WarningCount));
		result.SetPayloadValue("error_count", CountToString(report.ErrorCount));
		result.SetPayloadValue("can_continue_automatically", report.IsOperational() ? "true" : "false");

		if (report.IncludesProject) {
			result.SetPayloadValue("project_status", std::string(ToString(report.ProjectResult.Status)));
			AppendChildDiagnostics(result, "project", report.ProjectResult);
		}
		if (report.IncludesScene) {
			result.SetPayloadValue("scene_status", std::string(ToString(report.SceneResult.Status)));
			AppendChildDiagnostics(result, "scene", report.SceneResult);
		}
		if (report.IncludesAssets) {
			result.SetPayloadValue("asset_status", std::string(ToString(report.AssetResult.Status)));
			AppendChildDiagnostics(result, "asset", report.AssetResult);
		}
		if (outReport) {
			*outReport = report;
		}

		return result;
	}
}
