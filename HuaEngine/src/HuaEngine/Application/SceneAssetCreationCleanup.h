#pragma once

#include <filesystem>
#include <span>
#include <system_error>
#include <utility>
#include <vector>

#include "HuaEngine/Core/ResultEnvelope.h"

namespace HE {
	inline ResultEnvelope CleanupSceneAssetCreationFailure(
		ResultEnvelope failure,
		std::span<const std::filesystem::path> createdPaths) {
		failure.Operation = "asset.scene.create";
		std::vector<DiagnosticEntry> cleanupDiagnostics;
		for (const auto& path : createdPaths) {
			if (path.empty()) continue;
			std::error_code errorCode;
			std::filesystem::remove(path, errorCode);
			if (errorCode) {
				cleanupDiagnostics.push_back({
					DiagnosticSeverity::Error,
					"asset.scene.create.cleanup_failed",
					errorCode.message(),
					path.generic_string()
				});
			}
		}

		if (cleanupDiagnostics.empty()) return failure;

		auto result = ResultEnvelope::ManualIntervention(
			"asset.scene.create",
			failure.Target,
			"Scene asset creation failed and cleanup was incomplete");
		result.Payload = std::move(failure.Payload);
		result.Details = std::move(failure.Details);
		for (auto& diagnostic : cleanupDiagnostics) result.AddDetail(std::move(diagnostic));
		return result;
	}
}
