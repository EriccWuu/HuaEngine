#include "enginepch.h"
#include "CLISceneCommands.h"

#include <algorithm>
#include <array>
#include <cctype>

#include "HuaEngine/Project/ProjectContext.h"
#include "HuaEngine/Scene/Scene.h"

namespace HE::CLI {
	namespace {
		[[nodiscard]] std::optional<ProjectContext> ResolveOptionalProjectContext(
			const CLIParsedOptions& options,
			CLICommandContext& context,
			ResultEnvelope& outError) {
			if (!options.GetValue("--project").has_value()) {
				return std::nullopt;
			}

			ProjectContext resolvedContext;
			if (!ResolveProjectContext(context.Operations, options.GetValue("--project"), context.WorkingDirectory, resolvedContext, outError)) {
				return std::nullopt;
			}

			return resolvedContext;
		}
	}

	std::string SanitizeFileStem(std::string_view value) {
		std::string fileStem;
		fileStem.reserve(value.size());

		for (const char character : value) {
			if (std::isalnum(static_cast<unsigned char>(character))) {
				fileStem += static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
			}
			else if (character == ' ' || character == '-' || character == '_') {
				fileStem += '_';
			}
		}

		if (fileStem.empty()) {
			return "scene";
		}

		return fileStem;
	}

	std::optional<uint32_t> ParseEntityId(std::string_view value) {
		try {
			size_t consumed = 0;
			const auto parsed = std::stoul(std::string(value), &consumed, 10);
			if (consumed != value.size()) {
				return std::nullopt;
			}
			return static_cast<uint32_t>(parsed);
		}
		catch (...) {
			return std::nullopt;
		}
	}

	std::optional<SceneComponentKind> ParseSceneComponentKind(std::string_view value) {
		if (value == "camera") {
			return SceneComponentKind::Camera;
		}
		if (value == "mesh") {
			return SceneComponentKind::Mesh;
		}
		if (value == "material") {
			return SceneComponentKind::Material;
		}

		return std::nullopt;
	}

	CLICommandResponse RunSceneCommand(
		const CLICommandDefinition& command,
		const CLIParsedOptions& options,
		CLICommandContext& context) {
		if (command.Path == std::vector<std::string>{ "scene", "create" }) {
			const auto sceneName = options.GetValue("--name");
			if (!sceneName.has_value() || sceneName->empty()) {
				return { MakeUsageError("scene create requires --name") };
			}

			ProjectContext projectContext;
			ResultEnvelope resolveResult;
			if (!ResolveProjectContext(context.Operations, options.GetValue("--project"), context.WorkingDirectory, projectContext, resolveResult)) {
				return { std::move(resolveResult) };
			}

			Ref<Scene> scene;
			auto createResult = context.Operations.CreateScene(*sceneName, scene);
			if (!createResult.Succeeded()) {
				return { std::move(createResult) };
			}

			const auto defaultFileName = SanitizeFileStem(*sceneName) + ".scene";
			const auto outputPath = ResolveProjectRelativePath(
				std::filesystem::path(options.GetValue("--output").value_or(defaultFileName)),
				projectContext.GetSceneRootPath());
			auto saveResult = context.Operations.SaveScene(*scene, outputPath);
			if (!saveResult.Succeeded()) {
				return { std::move(saveResult) };
			}

			auto result = ResultEnvelope::Success("scene.create", outputPath.generic_string(), "Scene created and saved");
			result.SetPayloadValue("scene_name", scene->GetName());
			result.SetPayloadValue("scene_path", outputPath.generic_string());
			CopyPayloadIfMissing(result, createResult);
			CopyPayloadIfMissing(result, saveResult);
			MergeDetails(result, createResult);
			MergeDetails(result, saveResult);
			return { std::move(result) };
		}

		if (command.Path == std::vector<std::string>{ "scene", "validate" }) {
			const auto sceneArgument = options.GetValue("--scene");
			if (!sceneArgument.has_value() || sceneArgument->empty()) {
				return { MakeUsageError("scene validate requires --scene") };
			}

			ResultEnvelope resolveResult;
			const auto projectContext = ResolveOptionalProjectContext(options, context, resolveResult);
			if (options.GetValue("--project").has_value() && !resolveResult.Succeeded()) {
				return { std::move(resolveResult) };
			}

			const auto scenePath = ResolveScenePath(*sceneArgument, projectContext, context.WorkingDirectory);
			Ref<Scene> scene;
			auto loadResult = context.Operations.LoadScene(scenePath, scene);
			if (!loadResult.Succeeded()) {
				return { std::move(loadResult) };
			}

			auto result = context.Operations.ValidateScene(*scene);
			result.SetPayloadValue("scene_path", scenePath.generic_string());
			return { std::move(result) };
		}

		if (command.Path == std::vector<std::string>{ "scene", "entity", "create" }) {
			const auto sceneArgument = options.GetValue("--scene");
			if (!sceneArgument.has_value() || sceneArgument->empty()) {
				return { MakeUsageError("scene entity create requires --scene") };
			}

			const auto entityName = options.GetValue("--name");
			if (!entityName.has_value() || entityName->empty()) {
				return { MakeUsageError("scene entity create requires --name") };
			}

			ResultEnvelope resolveResult;
			const auto projectContext = ResolveOptionalProjectContext(options, context, resolveResult);
			if (options.GetValue("--project").has_value() && !resolveResult.Succeeded()) {
				return { std::move(resolveResult) };
			}

			const auto inputScenePath = ResolveScenePath(*sceneArgument, projectContext, context.WorkingDirectory);
			Ref<Scene> scene;
			auto loadResult = context.Operations.LoadScene(inputScenePath, scene);
			if (!loadResult.Succeeded()) {
				return { std::move(loadResult) };
			}

			uint32_t entityId = 0;
			auto createResult = context.Operations.CreateSceneEntity(*scene, *entityName, &entityId);
			if (!createResult.Succeeded()) {
				return { std::move(createResult) };
			}

			const auto outputScenePath = options.GetValue("--output").has_value()
				? ResolveScenePath(*options.GetValue("--output"), projectContext, context.WorkingDirectory)
				: inputScenePath;
			auto saveResult = context.Operations.SaveScene(*scene, outputScenePath);
			if (!saveResult.Succeeded()) {
				return { std::move(saveResult) };
			}

			auto result = ResultEnvelope::Success("scene.entity.create", outputScenePath.generic_string(), "Scene entity created and saved");
			result.SetPayloadValue("scene_path", outputScenePath.generic_string());
			result.SetPayloadValue("entity_id", std::to_string(entityId));
			CopyPayloadIfMissing(result, createResult);
			CopyPayloadIfMissing(result, saveResult);
			MergeDetails(result, createResult);
			MergeDetails(result, saveResult);
			return { std::move(result) };
		}

		if (command.Path == std::vector<std::string>{ "scene", "entity", "delete" }) {
			const auto sceneArgument = options.GetValue("--scene");
			if (!sceneArgument.has_value() || sceneArgument->empty()) {
				return { MakeUsageError("scene entity delete requires --scene") };
			}

			const auto entityIdArgument = options.GetValue("--entity-id");
			if (!entityIdArgument.has_value() || entityIdArgument->empty()) {
				return { MakeUsageError("scene entity delete requires --entity-id") };
			}

			const auto entityId = ParseEntityId(*entityIdArgument);
			if (!entityId.has_value()) {
				return { MakeUsageError("scene entity delete received an invalid --entity-id", *entityIdArgument) };
			}

			ResultEnvelope resolveResult;
			const auto projectContext = ResolveOptionalProjectContext(options, context, resolveResult);
			if (options.GetValue("--project").has_value() && !resolveResult.Succeeded()) {
				return { std::move(resolveResult) };
			}

			const auto inputScenePath = ResolveScenePath(*sceneArgument, projectContext, context.WorkingDirectory);
			Ref<Scene> scene;
			auto loadResult = context.Operations.LoadScene(inputScenePath, scene);
			if (!loadResult.Succeeded()) {
				return { std::move(loadResult) };
			}

			const std::array<uint32_t, 1> entityIds = { *entityId };
			auto deleteResult = context.Operations.DeleteSceneEntities(*scene, entityIds);
			if (!deleteResult.Succeeded()) {
				return { std::move(deleteResult) };
			}

			const auto outputScenePath = options.GetValue("--output").has_value()
				? ResolveScenePath(*options.GetValue("--output"), projectContext, context.WorkingDirectory)
				: inputScenePath;
			auto saveResult = context.Operations.SaveScene(*scene, outputScenePath);
			if (!saveResult.Succeeded()) {
				return { std::move(saveResult) };
			}

			auto result = ResultEnvelope::Success("scene.entity.delete", outputScenePath.generic_string(), "Scene entity deleted and saved");
			result.SetPayloadValue("scene_path", outputScenePath.generic_string());
			result.SetPayloadValue("entity_id", std::to_string(*entityId));
			CopyPayloadIfMissing(result, deleteResult);
			CopyPayloadIfMissing(result, saveResult);
			MergeDetails(result, deleteResult);
			MergeDetails(result, saveResult);
			return { std::move(result) };
		}

		if (command.Path == std::vector<std::string>{ "scene", "component", "add" } ||
			command.Path == std::vector<std::string>{ "scene", "component", "remove" }) {
			const auto sceneArgument = options.GetValue("--scene");
			if (!sceneArgument.has_value() || sceneArgument->empty()) {
				return { MakeUsageError("scene component command requires --scene") };
			}

			const auto entityIdArgument = options.GetValue("--entity-id");
			if (!entityIdArgument.has_value() || entityIdArgument->empty()) {
				return { MakeUsageError("scene component command requires --entity-id") };
			}

			const auto entityId = ParseEntityId(*entityIdArgument);
			if (!entityId.has_value()) {
				return { MakeUsageError("scene component command received an invalid --entity-id", *entityIdArgument) };
			}

			const auto componentArgument = options.GetValue("--component");
			if (!componentArgument.has_value() || componentArgument->empty()) {
				return { MakeUsageError("scene component command requires --component") };
			}

			const auto componentKind = ParseSceneComponentKind(*componentArgument);
			if (!componentKind.has_value()) {
				return { MakeUsageError("Unsupported scene component kind", *componentArgument) };
			}

			ResultEnvelope resolveResult;
			const auto projectContext = ResolveOptionalProjectContext(options, context, resolveResult);
			if (options.GetValue("--project").has_value() && !resolveResult.Succeeded()) {
				return { std::move(resolveResult) };
			}

			const auto inputScenePath = ResolveScenePath(*sceneArgument, projectContext, context.WorkingDirectory);
			Ref<Scene> scene;
			auto loadResult = context.Operations.LoadScene(inputScenePath, scene);
			if (!loadResult.Succeeded()) {
				return { std::move(loadResult) };
			}

			const bool isAdd = command.Path[2] == "add";
			auto mutationResult = isAdd
				? context.Operations.AddSceneComponent(*scene, *entityId, *componentKind)
				: context.Operations.RemoveSceneComponent(*scene, *entityId, *componentKind);
			if (!mutationResult.Succeeded()) {
				return { std::move(mutationResult) };
			}

			const auto outputScenePath = options.GetValue("--output").has_value()
				? ResolveScenePath(*options.GetValue("--output"), projectContext, context.WorkingDirectory)
				: inputScenePath;
			auto saveResult = context.Operations.SaveScene(*scene, outputScenePath);
			if (!saveResult.Succeeded()) {
				return { std::move(saveResult) };
			}

			auto result = ResultEnvelope::Success(
				isAdd ? "scene.component.add" : "scene.component.remove",
				outputScenePath.generic_string(),
				isAdd ? "Scene component added and saved" : "Scene component removed and saved");
			result.SetPayloadValue("scene_path", outputScenePath.generic_string());
			result.SetPayloadValue("entity_id", std::to_string(*entityId));
			result.SetPayloadValue("component_kind", std::string(ToString(*componentKind)));
			CopyPayloadIfMissing(result, mutationResult);
			CopyPayloadIfMissing(result, saveResult);
			MergeDetails(result, mutationResult);
			MergeDetails(result, saveResult);
			return { std::move(result) };
		}

		return { MakeUsageError(command, "Unknown command") };
	}
}
