#include "enginepch.h"
#include "ProjectService.h"

#include <system_error>

#include "HuaEngine/Serialization/Serialization.h"

namespace {
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
	bool ProjectContext::IsLoaded() const {
		return !RootPath.empty() && !ProjectFilePath.empty();
	}

	std::filesystem::path ProjectContext::GetMetadataDirectoryPath() const {
		return ProjectFilePath.parent_path();
	}

	std::filesystem::path ProjectContext::GetAssetRootPath() const {
		return RootPath / Descriptor.AssetDirectory;
	}

	std::string ProjectContext::GetTargetId() const {
		if (RootPath.empty()) {
			return {};
		}

		return RootPath.generic_string();
	}

	bool ProjectStatusReport::IsOperational() const {
		return RootExists && MetadataDirectoryExists && ProjectFileExists && AssetDirectoryExists;
	}

	bool ProjectStatusReport::HasIssues() const {
		return !IsOperational();
	}

	std::filesystem::path ProjectService::GetMetadataDirectoryPath(const std::filesystem::path& rootPath) {
		return NormalizePath(rootPath) / ProjectDirectoryName;
	}

	std::filesystem::path ProjectService::GetProjectFilePath(const std::filesystem::path& rootPath) {
		return GetMetadataDirectoryPath(rootPath) / ProjectFileName;
	}

	ResultEnvelope ProjectService::InitializeProject(
		const std::filesystem::path& rootPath,
		ProjectContext* outContext,
		std::string_view projectName) const {
		auto normalizedRoot = NormalizePath(rootPath);
		auto targetId = normalizedRoot.generic_string();

		if (normalizedRoot.empty()) {
			auto result = ResultEnvelope::Failure("project.initialize", targetId, "Project root path is empty");
			result.AddDetail({ DiagnosticSeverity::Error, "project.root.empty", "Cannot initialize a project without a root path", {} });
			return result;
		}

		std::error_code errorCode;
		std::filesystem::create_directories(normalizedRoot, errorCode);
		if (errorCode) {
			auto result = ResultEnvelope::Failure("project.initialize", targetId, "Failed to create project root");
			result.AddDetail({ DiagnosticSeverity::Error, "project.root.create_failed", errorCode.message(), normalizedRoot.generic_string() });
			return result;
		}

		auto metadataDirectory = GetMetadataDirectoryPath(normalizedRoot);
		auto projectFilePath = GetProjectFilePath(normalizedRoot);
		if (std::filesystem::exists(projectFilePath, errorCode)) {
			ProjectDescriptor existingDescriptor;
			if (!Serialization::LoadFromJson(projectFilePath.string(), existingDescriptor)) {
				auto result = ResultEnvelope::ManualIntervention("project.initialize", targetId, "Project metadata exists but could not be loaded");
				result.AddDetail({ DiagnosticSeverity::Error, "project.metadata.load_failed", "Refusing to overwrite an unreadable project descriptor", projectFilePath.generic_string() });
				return result;
			}

			ProjectContext existingContext;
			existingContext.RootPath = normalizedRoot;
			existingContext.ProjectFilePath = projectFilePath;
			existingContext.Descriptor = existingDescriptor;
			if (outContext) {
				*outContext = existingContext;
			}

			auto result = ResultEnvelope::Success("project.initialize", existingContext.GetTargetId(), "Project is already initialized");
			result.SetPayloadValue("created", "false");
			result.SetPayloadValue("project_name", existingContext.Descriptor.Name);
			result.SetPayloadValue("project_file", existingContext.ProjectFilePath.generic_string());
			result.AddDetail({ DiagnosticSeverity::Warning, "project.initialize.already_exists", "Project marker already exists; reusing the current project context", existingContext.ProjectFilePath.generic_string() });
			return result;
		}

		std::filesystem::create_directories(metadataDirectory, errorCode);
		if (errorCode) {
			auto result = ResultEnvelope::Failure("project.initialize", targetId, "Failed to create project metadata directory");
			result.AddDetail({ DiagnosticSeverity::Error, "project.metadata_directory.create_failed", errorCode.message(), metadataDirectory.generic_string() });
			return result;
		}

		ProjectDescriptor descriptor;
		if (!projectName.empty()) {
			descriptor.Name = std::string(projectName);
		}
		else if (normalizedRoot.has_filename()) {
			descriptor.Name = normalizedRoot.filename().string();
		}

		if (!Serialization::SaveAsJson(descriptor, projectFilePath.string())) {
			auto result = ResultEnvelope::Failure("project.initialize", targetId, "Failed to persist project metadata");
			result.AddDetail({ DiagnosticSeverity::Error, "project.metadata.save_failed", "Project descriptor could not be serialized to disk", projectFilePath.generic_string() });
			return result;
		}

		auto assetRoot = normalizedRoot / descriptor.AssetDirectory;
		std::filesystem::create_directories(assetRoot, errorCode);
		if (errorCode) {
			auto result = ResultEnvelope::Failure("project.initialize", targetId, "Failed to create asset directory");
			result.AddDetail({ DiagnosticSeverity::Error, "project.assets.create_failed", errorCode.message(), assetRoot.generic_string() });
			return result;
		}

		ProjectContext context;
		context.RootPath = normalizedRoot;
		context.ProjectFilePath = projectFilePath;
		context.Descriptor = descriptor;

		if (outContext) {
			*outContext = context;
		}

		auto result = ResultEnvelope::Success("project.initialize", context.GetTargetId(), "Project initialized");
		result.SetPayloadValue("created", "true");
		result.SetPayloadValue("project_name", context.Descriptor.Name);
		result.SetPayloadValue("project_file", context.ProjectFilePath.generic_string());
		result.SetPayloadValue("asset_root", context.GetAssetRootPath().generic_string());
		return result;
	}

	ResultEnvelope ProjectService::ResolveProjectContext(
		const std::filesystem::path& startingPath,
		ProjectContext& outContext) const {
		auto normalizedStart = NormalizePath(startingPath);
		auto targetId = normalizedStart.generic_string();

		if (normalizedStart.empty()) {
			auto result = ResultEnvelope::Failure("project.resolve_context", targetId, "Starting path is empty");
			result.AddDetail({ DiagnosticSeverity::Error, "project.resolve_context.empty_path", "Cannot resolve a project from an empty path", {} });
			return result;
		}

		std::error_code errorCode;
		auto cursor = std::filesystem::is_directory(normalizedStart, errorCode)
			? normalizedStart
			: normalizedStart.parent_path();

		while (!cursor.empty()) {
			auto projectFilePath = GetProjectFilePath(cursor);
			if (std::filesystem::exists(projectFilePath, errorCode)) {
				ProjectDescriptor descriptor;
				if (!Serialization::LoadFromJson(projectFilePath.string(), descriptor)) {
					auto result = ResultEnvelope::ManualIntervention("project.resolve_context", projectFilePath.generic_string(), "Project metadata exists but could not be loaded");
					result.AddDetail({ DiagnosticSeverity::Error, "project.metadata.load_failed", "Project descriptor could not be deserialized", projectFilePath.generic_string() });
					return result;
				}

				outContext.RootPath = NormalizePath(cursor);
				outContext.ProjectFilePath = projectFilePath;
				outContext.Descriptor = descriptor;

				auto result = ResultEnvelope::Success("project.resolve_context", outContext.GetTargetId(), "Project context resolved");
				result.SetPayloadValue("project_name", outContext.Descriptor.Name);
				result.SetPayloadValue("project_file", outContext.ProjectFilePath.generic_string());
				result.SetPayloadValue("asset_root", outContext.GetAssetRootPath().generic_string());
				return result;
			}

			auto parent = cursor.parent_path();
			if (parent == cursor) {
				break;
			}

			cursor = parent;
		}

		auto result = ResultEnvelope::Failure("project.resolve_context", targetId, "Project marker was not found");
		result.AddDetail({ DiagnosticSeverity::Error, "project.marker.missing", "No .huaengine/project.json marker was found while walking parent directories", normalizedStart.generic_string() });
		return result;
	}

	ResultEnvelope ProjectService::CheckProjectStatus(
		const ProjectContext& context,
		ProjectStatusReport* outReport) const {
		ProjectStatusReport report;

		if (context.RootPath.empty()) {
			auto result = ResultEnvelope::Failure("project.status", context.GetTargetId(), "Project context is not loaded");
			result.AddDetail({ DiagnosticSeverity::Error, "project.context.unloaded", "Project status requires a resolved project context", {} });
			if (outReport) {
				*outReport = report;
			}
			return result;
		}

		std::error_code errorCode;
		report.RootExists = std::filesystem::is_directory(context.RootPath, errorCode);
		report.MetadataDirectoryExists = std::filesystem::is_directory(context.GetMetadataDirectoryPath(), errorCode);
		report.ProjectFileExists = std::filesystem::is_regular_file(context.ProjectFilePath, errorCode);
		report.AssetDirectoryExists = std::filesystem::is_directory(context.GetAssetRootPath(), errorCode);

		if (outReport) {
			*outReport = report;
		}

		auto result = report.IsOperational()
			? ResultEnvelope::Success("project.status", context.GetTargetId(), "Project is operational")
			: ResultEnvelope::ManualIntervention("project.status", context.GetTargetId(), "Project is partially configured");

		result.SetPayloadValue("project_name", context.Descriptor.Name);
		result.SetPayloadValue("project_file", context.ProjectFilePath.generic_string());
		result.SetPayloadValue("root_exists", report.RootExists ? "true" : "false");
		result.SetPayloadValue("metadata_directory_exists", report.MetadataDirectoryExists ? "true" : "false");
		result.SetPayloadValue("project_file_exists", report.ProjectFileExists ? "true" : "false");
		result.SetPayloadValue("asset_directory_exists", report.AssetDirectoryExists ? "true" : "false");

		if (!report.RootExists) {
			result.AddDetail({ DiagnosticSeverity::Error, "project.root.invalid", "Project root is missing or is not a directory", context.RootPath.generic_string() });
		}
		if (!report.MetadataDirectoryExists) {
			result.AddDetail({ DiagnosticSeverity::Error, "project.metadata_directory.invalid", "Project metadata directory is missing or is not a directory", context.GetMetadataDirectoryPath().generic_string() });
		}
		if (!report.ProjectFileExists) {
			result.AddDetail({ DiagnosticSeverity::Error, "project.metadata.invalid", "Project metadata file is missing or is not a regular file", context.ProjectFilePath.generic_string() });
		}
		if (!report.AssetDirectoryExists) {
			result.AddDetail({ DiagnosticSeverity::Warning, "project.assets.invalid", "Asset directory is missing or is not a directory", context.GetAssetRootPath().generic_string() });
		}

		return result;
	}
}
