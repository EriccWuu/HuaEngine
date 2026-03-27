#include "enginepch.h"
#include "AgentHostAdapter.h"

#include <filesystem>
#include <optional>
#include <string_view>
#include <system_error>

#include "HuaEngine/Asset/AssetRegistry.h"
#include "HuaEngine/Project/ProjectContext.h"
#include "HuaEngine/Scene/Scene.h"

namespace {
	using HE::DiagnosticSeverity;
	using HE::ProjectContext;
	using HE::ResultEnvelope;

	std::filesystem::path NormalizePath(const std::filesystem::path& path) {
		if (path.empty()) {
			return {};
		}

		std::error_code errorCode;
		auto absolutePath = std::filesystem::absolute(path, errorCode);
		if (errorCode) {
			return path.lexically_normal();
		}

		return absolutePath.lexically_normal();
	}

	ResultEnvelope MakeRequestFailure(std::string_view operation, std::string_view summary, std::string_view context = {}) {
		auto result = ResultEnvelope::Failure(std::string(operation), "agent_host", std::string(summary));
		if (!context.empty()) {
			result.AddDetail({ DiagnosticSeverity::Error, "agent_host.invalid_request", std::string(summary), std::string(context) });
		}
		return result;
	}

	void MergeDetails(ResultEnvelope& destination, const ResultEnvelope& source) {
		for (const auto& detail : source.Details) {
			destination.Details.push_back(detail);
		}
	}

	void CopyPayloadIfMissing(ResultEnvelope& destination, const ResultEnvelope& source) {
		for (const auto& [key, value] : source.Payload) {
			destination.Payload.try_emplace(key, value);
		}
	}

	std::optional<std::string> GetArgument(const HE::ResultPayload& arguments, std::string_view key) {
		auto existing = arguments.find(std::string(key));
		if (existing == arguments.end()) {
			return std::nullopt;
		}

		return existing->second;
	}

	bool ResolveProjectContext(
		HE::ApplicationOperations& operations,
		const HE::ResultPayload& arguments,
		const std::filesystem::path& workingDirectory,
		ProjectContext& outContext,
		ResultEnvelope& outError) {
		const auto requestedPath = GetArgument(arguments, "path").value_or(
			GetArgument(arguments, "project_path").value_or(workingDirectory.string()));
		outError = operations.ResolveProjectContext(NormalizePath(requestedPath), outContext);
		return outError.Succeeded();
	}

	std::optional<HE::BuiltinMeshPrimitive> ParsePrimitive(std::string_view primitive) {
		if (primitive == "quad") {
			return HE::BuiltinMeshPrimitive::Quad;
		}
		if (primitive == "cube") {
			return HE::BuiltinMeshPrimitive::Cube;
		}
		if (primitive == "sphere") {
			return HE::BuiltinMeshPrimitive::Sphere;
		}

		return std::nullopt;
	}
}

namespace HE {
	AgentHostAdapter::AgentHostAdapter(ApplicationOperations& operations)
		: m_Operations(&operations) {}

	AgentOperationResponse AgentHostAdapter::Invoke(const AgentOperationRequest& request) const {
		if (!m_Operations) {
			return { MakeRequestFailure("agent.invoke", "Application operations are not available") };
		}

		if (request.Operation == "ops.list") {
			auto result = ResultEnvelope::Success("ops.list", "operation_registry", "Formal operations listed for agent host");
			result.SetPayloadValue("operation_count", std::to_string(m_Operations->GetRegistry().Size()));
			return { std::move(result), m_Operations->GetRegistry().List() };
		}

		if (request.Operation == "project.initialize") {
			ProjectContext context;
			auto result = m_Operations->InitializeProject(
				NormalizePath(GetArgument(request.Arguments, "root").value_or(request.WorkingDirectory.string())),
				&context,
				GetArgument(request.Arguments, "name").value_or(std::string()));
			return { std::move(result) };
		}

		if (request.Operation == "project.check_status") {
			ProjectContext context;
			ResultEnvelope resolveError;
			if (!ResolveProjectContext(*m_Operations, request.Arguments, request.WorkingDirectory, context, resolveError)) {
				return { std::move(resolveError) };
			}

			return { m_Operations->CheckProjectStatus(context) };
		}

		if (request.Operation == "scene.create") {
			const auto sceneName = GetArgument(request.Arguments, "scene_name");
			if (!sceneName.has_value() || sceneName->empty()) {
				return { MakeRequestFailure(request.Operation, "scene_name is required") };
			}

			ProjectContext context;
			ResultEnvelope resolveError;
			if (!ResolveProjectContext(*m_Operations, request.Arguments, request.WorkingDirectory, context, resolveError)) {
				return { std::move(resolveError) };
			}

			Ref<Scene> scene;
			auto createResult = m_Operations->CreateScene(*sceneName, scene);
			if (!createResult.Succeeded()) {
				return { std::move(createResult) };
			}

			const auto outputPath = context.GetSceneRootPath() / GetArgument(request.Arguments, "output").value_or(*sceneName + ".scene");
			auto saveResult = m_Operations->SaveScene(*scene, outputPath);
			if (!saveResult.Succeeded()) {
				return { std::move(saveResult) };
			}

			auto result = createResult;
			result.Target = outputPath.generic_string();
			result.Summary = saveResult.Summary;
			CopyPayloadIfMissing(result, saveResult);
			MergeDetails(result, saveResult);
			result.SetPayloadValue("scene_path", outputPath.generic_string());
			return { std::move(result) };
		}

		if (request.Operation == "asset.create_builtin_mesh") {
			const auto assetId = GetArgument(request.Arguments, "asset_id");
			if (!assetId.has_value() || assetId->empty()) {
				return { MakeRequestFailure(request.Operation, "asset_id is required") };
			}

			const auto primitive = ParsePrimitive(GetArgument(request.Arguments, "primitive").value_or("quad"));
			if (!primitive.has_value()) {
				return { MakeRequestFailure(request.Operation, "Unsupported built-in mesh primitive") };
			}

			ProjectContext context;
			ResultEnvelope resolveError;
			if (!ResolveProjectContext(*m_Operations, request.Arguments, request.WorkingDirectory, context, resolveError)) {
				return { std::move(resolveError) };
			}

			return {
				m_Operations->CreateBuiltinMeshAsset(
					context,
					*assetId,
					*primitive,
					GetArgument(request.Arguments, "mesh_name").value_or(std::string()))
			};
		}

		if (request.Operation == "script.status") {
			const auto sceneArgument = GetArgument(request.Arguments, "scene");
			if (!sceneArgument.has_value() || sceneArgument->empty()) {
				return { MakeRequestFailure(request.Operation, "scene is required") };
			}

			ProjectContext context;
			ResultEnvelope resolveError;
			if (!ResolveProjectContext(*m_Operations, request.Arguments, request.WorkingDirectory, context, resolveError)) {
				return { std::move(resolveError) };
			}

			Ref<Scene> scene;
			auto loadResult = m_Operations->LoadScene(context.GetSceneRootPath() / *sceneArgument, scene);
			if (!loadResult.Succeeded()) {
				return { std::move(loadResult) };
			}

			auto result = m_Operations->CheckSceneScripts(*scene);
			result.SetPayloadValue("scene_path", (context.GetSceneRootPath() / *sceneArgument).generic_string());
			return { std::move(result) };
		}

		if (request.Operation == "validation.validate") {
			ProjectContext context;
			ResultEnvelope resolveError;
			if (!ResolveProjectContext(*m_Operations, request.Arguments, request.WorkingDirectory, context, resolveError)) {
				return { std::move(resolveError) };
			}

			ApplicationValidationRequest validationRequest;
			validationRequest.Project = &context;
			validationRequest.IncludeAssets = GetArgument(request.Arguments, "include_assets").value_or("false") == "true";
			validationRequest.IncludeScripts = GetArgument(request.Arguments, "include_scripts").value_or("false") == "true";

			Ref<Scene> scene;
			if (const auto sceneArgument = GetArgument(request.Arguments, "scene"); sceneArgument.has_value() && !sceneArgument->empty()) {
				auto loadResult = m_Operations->LoadScene(context.GetSceneRootPath() / *sceneArgument, scene);
				if (!loadResult.Succeeded()) {
					return { std::move(loadResult) };
				}

				validationRequest.SceneTarget = scene.get();
				validationRequest.ScriptScene = validationRequest.IncludeScripts ? scene.get() : nullptr;
			}

			auto result = m_Operations->Validate(validationRequest);
			result.SetPayloadValue("project_root", context.RootPath.generic_string());
			if (scene) {
				result.SetPayloadValue("scene_path", (context.GetSceneRootPath() / *GetArgument(request.Arguments, "scene")).generic_string());
			}
			return { std::move(result) };
		}

		return { MakeRequestFailure(request.Operation, "Unsupported agent operation", request.Operation) };
	}
}
