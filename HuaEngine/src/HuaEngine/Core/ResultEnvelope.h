#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "HuaEngine/Core/Core.h"

namespace HE {
	enum class OperationStatus {
		// Operation completed and the caller can continue automatically.
		Success,
		// Operation failed and the caller should treat the result as a blocking non-success.
		Failure,
		// Operation reached a terminal state that requires explicit human decision before continuing.
		ManualInterventionRequired
	};

	enum class DiagnosticSeverity {
		Info,
		Warning,
		Error
	};

	using ResultPayload = std::unordered_map<std::string, std::string>;

	struct DiagnosticEntry {
		DiagnosticSeverity Severity = DiagnosticSeverity::Info;
		std::string Code;
		std::string Message;
		std::string Context;
	};

	struct ResultEnvelope {
		// Use stable, dot-separated identifiers such as "scene.save" or "asset.validate".
		std::string Operation;
		// Use stable resource-like identifiers such as a scene path, asset handle, or project id.
		std::string Target;
		OperationStatus Status = OperationStatus::Success;
		std::string Summary;
		ResultPayload Payload;
		std::vector<DiagnosticEntry> Details;

		[[nodiscard]] bool Succeeded() const {
			return Status == OperationStatus::Success;
		}

		[[nodiscard]] bool Failed() const {
			return Status != OperationStatus::Success;
		}

		[[nodiscard]] bool RequiresManualIntervention() const {
			return Status == OperationStatus::ManualInterventionRequired;
		}

		[[nodiscard]] bool CanContinueAutomatically() const {
			return Status == OperationStatus::Success;
		}

		void AddDetail(DiagnosticEntry detail) {
			Details.emplace_back(std::move(detail));
		}

		void SetPayloadValue(std::string key, std::string value) {
			Payload[std::move(key)] = std::move(value);
		}

		static ResultEnvelope Success(std::string operation, std::string target, std::string summary = {}) {
			return {
				.Operation = std::move(operation),
				.Target = std::move(target),
				.Status = OperationStatus::Success,
				.Summary = std::move(summary)
			};
		}

		static ResultEnvelope Failure(std::string operation, std::string target, std::string summary = {}) {
			return {
				.Operation = std::move(operation),
				.Target = std::move(target),
				.Status = OperationStatus::Failure,
				.Summary = std::move(summary)
			};
		}

		static ResultEnvelope ManualIntervention(std::string operation, std::string target, std::string summary = {}) {
			return {
				.Operation = std::move(operation),
				.Target = std::move(target),
				.Status = OperationStatus::ManualInterventionRequired,
				.Summary = std::move(summary)
			};
		}
	};

	inline constexpr std::string_view ToString(OperationStatus status) {
		switch (status) {
		case OperationStatus::Success:
			return "success";
		case OperationStatus::Failure:
			return "failure";
		case OperationStatus::ManualInterventionRequired:
			return "manual_intervention_required";
		}

		return "unknown";
	}

	inline constexpr std::string_view ToString(DiagnosticSeverity severity) {
		switch (severity) {
		case DiagnosticSeverity::Info:
			return "info";
		case DiagnosticSeverity::Warning:
			return "warning";
		case DiagnosticSeverity::Error:
			return "error";
		}

		return "unknown";
	}
}
