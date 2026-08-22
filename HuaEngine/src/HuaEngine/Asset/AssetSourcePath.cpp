#include "enginepch.h"
#include "AssetSourcePath.h"

#include "HuaEngine/Core/ResourcePaths.h"

namespace {
	bool IsPathWithinRoot(
		const std::filesystem::path& root,
		const std::filesystem::path& candidate) {
		const auto relativePath = candidate.lexically_relative(root);
		return !relativePath.empty() && HE::IsSafeAssetRelativePath(relativePath);
	}
}

namespace HE {
	bool IsSafeAssetRelativePath(const std::filesystem::path& path) {
		if (path.empty() || path == "." || path.is_absolute() || path.has_root_name()) {
			return false;
		}

		for (const auto& component : path.lexically_normal()) {
			if (component == "..") {
				return false;
			}
		}
		return true;
	}

	ResultEnvelope ResolveAssetSourcePath(
		const ProjectContext& context,
		const AssetManifestRecord& record,
		std::filesystem::path& outSourcePath) {
		outSourcePath.clear();
		if (!IsSafeAssetRelativePath(record.RelativePath)) {
			auto result = ResultEnvelope::Failure("asset.source.resolve", record.Guid, "Asset source path is invalid");
			result.AddDetail({
				DiagnosticSeverity::Error,
				"asset.source.path_invalid",
				"Asset source path must remain relative to its source root",
				record.RelativePath.generic_string()
			});
			return result;
		}

		std::filesystem::path sourceRoot;
		switch (record.Source) {
		case AssetSource::File:
			sourceRoot = context.GetAssetRootPath();
			break;
		case AssetSource::Builtin:
			sourceRoot = ResourcePaths::GetEngineResourceRoot() / "BuiltinAssets";
			break;
		case AssetSource::Unknown:
		default: {
			auto result = ResultEnvelope::Failure("asset.source.resolve", record.Guid, "Asset source type is unsupported");
			result.AddDetail({
				DiagnosticSeverity::Error,
				"asset.source.unsupported",
				"Asset source path cannot be resolved for this source type",
				std::string(ToString(record.Source))
			});
			return result;
		}
		}

		std::error_code errorCode;
		sourceRoot = std::filesystem::absolute(sourceRoot, errorCode).lexically_normal();
		if (errorCode) {
			auto result = ResultEnvelope::Failure("asset.source.resolve", record.Guid, "Asset source root could not be resolved");
			result.AddDetail({ DiagnosticSeverity::Error, "asset.source.root_invalid", errorCode.message(), sourceRoot.generic_string() });
			return result;
		}

		const auto candidate = (sourceRoot / record.RelativePath).lexically_normal();
		if (!IsPathWithinRoot(sourceRoot, candidate)) {
			auto result = ResultEnvelope::Failure("asset.source.resolve", record.Guid, "Asset source path escapes its source root");
			result.AddDetail({ DiagnosticSeverity::Error, "asset.source.path_escape", "Asset source path escapes its source root", candidate.generic_string() });
			return result;
		}

		outSourcePath = candidate;
		return ResultEnvelope::Success("asset.source.resolve", record.Guid, "Asset source path resolved");
	}
}
