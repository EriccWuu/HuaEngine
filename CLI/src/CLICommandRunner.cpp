#include "enginepch.h"
#include "CLICommandRunner.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <optional>
#include <span>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>

#include "HuaEngine/Project/ProjectContext.h"
#include "HuaEngine/Scene/Scene.h"

namespace {
	using HE::DiagnosticEntry;
	using HE::DiagnosticSeverity;
	using HE::ProjectContext;
	using HE::Ref;
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

		if (std::filesystem::exists(absolutePath, errorCode)) {
			auto canonicalPath = std::filesystem::weakly_canonical(absolutePath, errorCode);
			if (!errorCode) {
				return canonicalPath;
			}
		}

		return absolutePath.lexically_normal();
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

	ResultEnvelope MakeUsageError(std::string_view summary, std::string_view context = {}) {
		auto result = ResultEnvelope::Failure("cli.usage", "command_line", std::string(summary));
		result.AddDetail({ DiagnosticSeverity::Error, "cli.usage.invalid", std::string(summary), std::string(context) });
		result.AddDetail({ DiagnosticSeverity::Info, "cli.usage.example", "Supported commands include: ops list, project init, project status, scene create, scene validate, scene entity create/delete, scene component add/remove, asset register-default-mesh, asset validate, script status, script initialize, script update, script shutdown, validation run", {} });
		return result;
	}

	ResultEnvelope MakeHostFailure(std::string_view operation, std::string_view summary, std::string_view context = {}) {
		auto result = ResultEnvelope::Failure(std::string(operation), "command_line", std::string(summary));
		if (!context.empty()) {
			result.AddDetail({ DiagnosticSeverity::Error, "cli.host.failure", std::string(summary), std::string(context) });
		}
		return result;
	}

	class OptionParser {
	public:
		bool Parse(
			std::span<const std::string> tokens,
			const std::unordered_set<std::string>& valueOptions,
			const std::unordered_set<std::string>& flagOptions,
			ResultEnvelope& outError) {
			for (size_t index = 0; index < tokens.size(); ++index) {
				const auto& token = tokens[index];
				if (!token.starts_with("--")) {
					outError = MakeUsageError("Unexpected positional argument", token);
					return false;
				}

				if (flagOptions.contains(token)) {
					m_Flags.insert(token);
					continue;
				}

				if (!valueOptions.contains(token)) {
					outError = MakeUsageError("Unknown option", token);
					return false;
				}

				if (index + 1 >= tokens.size()) {
					outError = MakeUsageError("Option requires a value", token);
					return false;
				}

				const auto& value = tokens[++index];
				if (value.starts_with("--")) {
					outError = MakeUsageError("Option requires a value", token);
					return false;
				}

				m_Values[token] = value;
			}

			return true;
		}

		[[nodiscard]] bool HasFlag(std::string_view key) const {
			return m_Flags.contains(std::string(key));
		}

		[[nodiscard]] std::optional<std::string> GetValue(std::string_view key) const {
			auto existing = m_Values.find(std::string(key));
			if (existing == m_Values.end()) {
				return std::nullopt;
			}

			return existing->second;
		}

	private:
		std::unordered_map<std::string, std::string> m_Values;
		std::unordered_set<std::string> m_Flags;
	};

	bool ResolveProjectContext(
		HE::ApplicationOperations& operations,
		const std::optional<std::string>& explicitPath,
		const std::filesystem::path& workingDirectory,
		ProjectContext& outContext,
		ResultEnvelope& outError) {
		const auto basePath = NormalizePath(explicitPath.has_value() ? std::filesystem::path(*explicitPath) : workingDirectory);
		outError = operations.ResolveProjectContext(basePath, outContext);
		return outError.Succeeded();
	}

	std::filesystem::path ResolveProjectRelativePath(
		const std::filesystem::path& inputPath,
		const std::filesystem::path& rootPath) {
		if (inputPath.is_absolute()) {
			return NormalizePath(inputPath);
		}

		return NormalizePath(rootPath / inputPath);
	}

	std::filesystem::path ResolveScenePath(
		const std::string& sceneArgument,
		const std::optional<ProjectContext>& context,
		const std::filesystem::path& workingDirectory) {
		std::filesystem::path scenePath(sceneArgument);
		if (scenePath.is_absolute()) {
			return NormalizePath(scenePath);
		}

		if (context.has_value()) {
			return ResolveProjectRelativePath(scenePath, context->GetSceneRootPath());
		}

		return NormalizePath(workingDirectory / scenePath);
	}

	std::optional<HE::BuiltinMeshPrimitive> ParseBuiltinMeshPrimitive(std::string_view primitive) {
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

	std::optional<HE::SceneComponentKind> ParseSceneComponentKind(std::string_view value) {
		if (value == "camera") {
			return HE::SceneComponentKind::Camera;
		}
		if (value == "mesh") {
			return HE::SceneComponentKind::Mesh;
		}
		if (value == "material") {
			return HE::SceneComponentKind::Material;
		}

		return std::nullopt;
	}
}

namespace HE::CLI {
	CommandRunner::CommandRunner(ApplicationOperations& operations)
		: m_Operations(&operations)
	{
	}

	CLICommandResponse CommandRunner::Run(
		const std::vector<std::string>& arguments,
		const std::filesystem::path& workingDirectory) const {
		if (!m_Operations) {
			return { MakeHostFailure("cli.host", "Application operations are not available") };
		}

		if (arguments.empty()) {
			return { MakeUsageError("No command specified") };
		}

		const auto command = arguments[0];
		const auto subcommand = arguments.size() > 1 ? arguments[1] : std::string();
		const bool hasDetailCommand = arguments.size() > 2 && !arguments[2].starts_with("--");
		const auto detailCommand = hasDetailCommand ? arguments[2] : std::string();
		const size_t optionStartIndex = hasDetailCommand ? 3 : 2;
		const std::span<const std::string> optionTokens(
			arguments.data() + std::min(optionStartIndex, arguments.size()),
			arguments.size() > optionStartIndex ? arguments.size() - optionStartIndex : 0);

		if (command == "help") {
			auto result = ResultEnvelope::Success("cli.help", "command_line", "CLI command help");
			result.AddDetail({ DiagnosticSeverity::Info, "cli.help.summary", "Use 'ops list' to inspect the formal operation registry", {} });
			result.AddDetail({ DiagnosticSeverity::Info, "cli.help.commands", "Supported commands include: project init/status, scene create/validate, scene entity create/delete, scene component add/remove, asset register-default-mesh/validate, script status/initialize/update/shutdown, validation run", {} });
			return { std::move(result) };
		}

		if (command == "ops" && subcommand == "list") {
			if (!optionTokens.empty()) {
				return { MakeUsageError("ops list does not accept options") };
			}

			auto result = ResultEnvelope::Success("cli.ops_list", "operation_registry", "Formal operation registry listed");
			result.SetPayloadValue("operation_count", std::to_string(m_Operations->GetOperationRegistry().Size()));
			return { std::move(result), m_Operations->GetOperationRegistry().List() };
		}

		if (command == "project" && subcommand == "init") {
			OptionParser options;
			ResultEnvelope optionError;
			if (!options.Parse(optionTokens, { "--root", "--name" }, {}, optionError)) {
				return { std::move(optionError) };
			}

			ProjectContext context;
			auto result = m_Operations->InitializeProject(
				options.GetValue("--root").value_or(NormalizePath(workingDirectory).string()),
				&context,
				options.GetValue("--name").value_or(std::string()));
			return { std::move(result) };
		}

		if (command == "project" && subcommand == "status") {
			OptionParser options;
			ResultEnvelope optionError;
			if (!options.Parse(optionTokens, { "--path" }, {}, optionError)) {
				return { std::move(optionError) };
			}

			ProjectContext context;
			ResultEnvelope resolveResult;
			if (!ResolveProjectContext(*m_Operations, options.GetValue("--path"), workingDirectory, context, resolveResult)) {
				return { std::move(resolveResult) };
			}

			return { m_Operations->CheckProjectStatus(context) };
		}

		if (command == "scene" && subcommand == "create") {
			OptionParser options;
			ResultEnvelope optionError;
			if (!options.Parse(optionTokens, { "--project", "--name", "--output" }, {}, optionError)) {
				return { std::move(optionError) };
			}

			const auto sceneName = options.GetValue("--name");
			if (!sceneName.has_value() || sceneName->empty()) {
				return { MakeUsageError("scene create requires --name") };
			}

			ProjectContext context;
			ResultEnvelope resolveResult;
			if (!ResolveProjectContext(*m_Operations, options.GetValue("--project"), workingDirectory, context, resolveResult)) {
				return { std::move(resolveResult) };
			}

			Ref<Scene> scene;
			auto createResult = m_Operations->CreateScene(*sceneName, scene);
			if (!createResult.Succeeded()) {
				return { std::move(createResult) };
			}

			const auto defaultFileName = SanitizeFileStem(*sceneName) + ".scene";
			const auto outputPath = ResolveProjectRelativePath(
				std::filesystem::path(options.GetValue("--output").value_or(defaultFileName)),
				context.GetSceneRootPath());
			auto saveResult = m_Operations->SaveScene(*scene, outputPath);
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

		if (command == "scene" && subcommand == "validate") {
			OptionParser options;
			ResultEnvelope optionError;
			if (!options.Parse(optionTokens, { "--project", "--scene" }, {}, optionError)) {
				return { std::move(optionError) };
			}

			const auto sceneArgument = options.GetValue("--scene");
			if (!sceneArgument.has_value() || sceneArgument->empty()) {
				return { MakeUsageError("scene validate requires --scene") };
			}

			std::optional<ProjectContext> context;
			if (options.GetValue("--project").has_value()) {
				ProjectContext resolvedContext;
				ResultEnvelope resolveResult;
				if (!ResolveProjectContext(*m_Operations, options.GetValue("--project"), workingDirectory, resolvedContext, resolveResult)) {
					return { std::move(resolveResult) };
				}
				context = resolvedContext;
			}

			Ref<Scene> scene;
			auto loadResult = m_Operations->LoadScene(ResolveScenePath(*sceneArgument, context, workingDirectory), scene);
			if (!loadResult.Succeeded()) {
				return { std::move(loadResult) };
			}

			auto result = m_Operations->ValidateScene(*scene);
			result.SetPayloadValue("scene_path", ResolveScenePath(*sceneArgument, context, workingDirectory).generic_string());
			return { std::move(result) };
		}

		if (command == "scene" && subcommand == "entity" && detailCommand == "create") {
			OptionParser options;
			ResultEnvelope optionError;
			if (!options.Parse(optionTokens, { "--project", "--scene", "--name", "--output" }, {}, optionError)) {
				return { std::move(optionError) };
			}

			const auto sceneArgument = options.GetValue("--scene");
			if (!sceneArgument.has_value() || sceneArgument->empty()) {
				return { MakeUsageError("scene entity create requires --scene") };
			}

			const auto entityName = options.GetValue("--name");
			if (!entityName.has_value() || entityName->empty()) {
				return { MakeUsageError("scene entity create requires --name") };
			}

			std::optional<ProjectContext> context;
			if (options.GetValue("--project").has_value()) {
				ProjectContext resolvedContext;
				ResultEnvelope resolveResult;
				if (!ResolveProjectContext(*m_Operations, options.GetValue("--project"), workingDirectory, resolvedContext, resolveResult)) {
					return { std::move(resolveResult) };
				}
				context = resolvedContext;
			}

			const auto inputScenePath = ResolveScenePath(*sceneArgument, context, workingDirectory);
			Ref<Scene> scene;
			auto loadResult = m_Operations->LoadScene(inputScenePath, scene);
			if (!loadResult.Succeeded()) {
				return { std::move(loadResult) };
			}

			uint32_t entityId = 0;
			auto createResult = m_Operations->CreateSceneEntity(*scene, *entityName, &entityId);
			if (!createResult.Succeeded()) {
				return { std::move(createResult) };
			}

			const auto outputScenePath = options.GetValue("--output").has_value()
				? ResolveScenePath(*options.GetValue("--output"), context, workingDirectory)
				: inputScenePath;
			auto saveResult = m_Operations->SaveScene(*scene, outputScenePath);
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

		if (command == "scene" && subcommand == "entity" && detailCommand == "delete") {
			OptionParser options;
			ResultEnvelope optionError;
			if (!options.Parse(optionTokens, { "--project", "--scene", "--entity-id", "--output" }, {}, optionError)) {
				return { std::move(optionError) };
			}

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

			std::optional<ProjectContext> context;
			if (options.GetValue("--project").has_value()) {
				ProjectContext resolvedContext;
				ResultEnvelope resolveResult;
				if (!ResolveProjectContext(*m_Operations, options.GetValue("--project"), workingDirectory, resolvedContext, resolveResult)) {
					return { std::move(resolveResult) };
				}
				context = resolvedContext;
			}

			const auto inputScenePath = ResolveScenePath(*sceneArgument, context, workingDirectory);
			Ref<Scene> scene;
			auto loadResult = m_Operations->LoadScene(inputScenePath, scene);
			if (!loadResult.Succeeded()) {
				return { std::move(loadResult) };
			}

			const std::array<uint32_t, 1> entityIds = { *entityId };
			auto deleteResult = m_Operations->DeleteSceneEntities(*scene, entityIds);
			if (!deleteResult.Succeeded()) {
				return { std::move(deleteResult) };
			}

			const auto outputScenePath = options.GetValue("--output").has_value()
				? ResolveScenePath(*options.GetValue("--output"), context, workingDirectory)
				: inputScenePath;
			auto saveResult = m_Operations->SaveScene(*scene, outputScenePath);
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

		if (command == "scene" && subcommand == "component" && (detailCommand == "add" || detailCommand == "remove")) {
			OptionParser options;
			ResultEnvelope optionError;
			if (!options.Parse(optionTokens, { "--project", "--scene", "--entity-id", "--component", "--output" }, {}, optionError)) {
				return { std::move(optionError) };
			}

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

			std::optional<ProjectContext> context;
			if (options.GetValue("--project").has_value()) {
				ProjectContext resolvedContext;
				ResultEnvelope resolveResult;
				if (!ResolveProjectContext(*m_Operations, options.GetValue("--project"), workingDirectory, resolvedContext, resolveResult)) {
					return { std::move(resolveResult) };
				}
				context = resolvedContext;
			}

			const auto inputScenePath = ResolveScenePath(*sceneArgument, context, workingDirectory);
			Ref<Scene> scene;
			auto loadResult = m_Operations->LoadScene(inputScenePath, scene);
			if (!loadResult.Succeeded()) {
				return { std::move(loadResult) };
			}

			auto mutationResult = detailCommand == "add"
				? m_Operations->AddSceneComponent(*scene, *entityId, *componentKind)
				: m_Operations->RemoveSceneComponent(*scene, *entityId, *componentKind);
			if (!mutationResult.Succeeded()) {
				return { std::move(mutationResult) };
			}

			const auto outputScenePath = options.GetValue("--output").has_value()
				? ResolveScenePath(*options.GetValue("--output"), context, workingDirectory)
				: inputScenePath;
			auto saveResult = m_Operations->SaveScene(*scene, outputScenePath);
			if (!saveResult.Succeeded()) {
				return { std::move(saveResult) };
			}

			auto result = ResultEnvelope::Success(
				detailCommand == "add" ? "scene.component.add" : "scene.component.remove",
				outputScenePath.generic_string(),
				detailCommand == "add" ? "Scene component added and saved" : "Scene component removed and saved");
			result.SetPayloadValue("scene_path", outputScenePath.generic_string());
			result.SetPayloadValue("entity_id", std::to_string(*entityId));
			result.SetPayloadValue("component_kind", std::string(ToString(*componentKind)));
			CopyPayloadIfMissing(result, mutationResult);
			CopyPayloadIfMissing(result, saveResult);
			MergeDetails(result, mutationResult);
			MergeDetails(result, saveResult);
			return { std::move(result) };
		}

		if (command == "asset" && subcommand == "register-default-mesh") {
			OptionParser options;
			ResultEnvelope optionError;
			if (!options.Parse(optionTokens, { "--project", "--asset-id", "--primitive", "--name" }, {}, optionError)) {
				return { std::move(optionError) };
			}

			const auto assetId = options.GetValue("--asset-id");
			if (!assetId.has_value() || assetId->empty()) {
				return { MakeUsageError("asset register-default-mesh requires --asset-id") };
			}

			const auto primitive = options.GetValue("--primitive").value_or("quad");
			const auto meshName = options.GetValue("--name").value_or(primitive);
			const auto builtinPrimitive = ParseBuiltinMeshPrimitive(primitive);
			if (!builtinPrimitive.has_value()) {
				return { MakeUsageError("Unsupported mesh primitive", primitive) };
			}

			ProjectContext context;
			ResultEnvelope resolveResult;
			if (!ResolveProjectContext(*m_Operations, options.GetValue("--project"), workingDirectory, context, resolveResult)) {
				return { std::move(resolveResult) };
			}

			auto result = m_Operations->CreateBuiltinMeshAsset(context, *assetId, *builtinPrimitive, meshName);
			return { std::move(result) };
		}

		if (command == "asset" && subcommand == "validate") {
			OptionParser options;
			ResultEnvelope optionError;
			if (!options.Parse(optionTokens, { "--path" }, {}, optionError)) {
				return { std::move(optionError) };
			}

			ProjectContext context;
			ResultEnvelope resolveResult;
			if (!ResolveProjectContext(*m_Operations, options.GetValue("--path"), workingDirectory, context, resolveResult)) {
				return { std::move(resolveResult) };
			}

			return { m_Operations->ValidateAssets(context) };
		}

		if (command == "script" &&
			(subcommand == "status" || subcommand == "initialize" || subcommand == "update" || subcommand == "shutdown")) {
			OptionParser options;
			ResultEnvelope optionError;
			if (!options.Parse(optionTokens, { "--project", "--scene" }, {}, optionError)) {
				return { std::move(optionError) };
			}

			const auto sceneArgument = options.GetValue("--scene");
			if (!sceneArgument.has_value() || sceneArgument->empty()) {
				return { MakeUsageError("script command requires --scene") };
			}

			std::optional<ProjectContext> context;
			if (options.GetValue("--project").has_value()) {
				ProjectContext resolvedContext;
				ResultEnvelope resolveResult;
				if (!ResolveProjectContext(*m_Operations, options.GetValue("--project"), workingDirectory, resolvedContext, resolveResult)) {
					return { std::move(resolveResult) };
				}
				context = resolvedContext;
			}

			Ref<Scene> scene;
			auto loadResult = m_Operations->LoadScene(ResolveScenePath(*sceneArgument, context, workingDirectory), scene);
			if (!loadResult.Succeeded()) {
				return { std::move(loadResult) };
			}

			ResultEnvelope result;
			if (subcommand == "status") {
				result = m_Operations->CheckSceneScripts(*scene);
			}
			else {
				auto attachResult = m_Operations->AttachScriptRuntime(*scene);
				if (!attachResult.Succeeded()) {
					return { std::move(attachResult) };
				}

				if (subcommand == "initialize") {
					result = m_Operations->InitializeSceneScripts(*scene);
				}
				else if (subcommand == "update") {
					result = m_Operations->UpdateSceneScripts(*scene);
				}
				else {
					result = m_Operations->ShutdownSceneScripts(*scene);
				}
			}

			result.SetPayloadValue("scene_path", ResolveScenePath(*sceneArgument, context, workingDirectory).generic_string());
			return { std::move(result) };
		}

		if (command == "validation" && subcommand == "run") {
			OptionParser options;
			ResultEnvelope optionError;
			if (!options.Parse(optionTokens, { "--path", "--scene" }, { "--include-assets", "--include-scripts" }, optionError)) {
				return { std::move(optionError) };
			}

			if (options.HasFlag("--include-scripts") && !options.GetValue("--scene").has_value()) {
				return { MakeUsageError("validation run with --include-scripts requires --scene") };
			}

			ProjectContext context;
			ResultEnvelope resolveResult;
			if (!ResolveProjectContext(*m_Operations, options.GetValue("--path"), workingDirectory, context, resolveResult)) {
				return { std::move(resolveResult) };
			}

			ApplicationValidationRequest request;
			request.Project = &context;
			request.IncludeAssets = options.HasFlag("--include-assets");

			Ref<Scene> scene;
			if (const auto sceneArgument = options.GetValue("--scene"); sceneArgument.has_value()) {
				auto loadResult = m_Operations->LoadScene(ResolveScenePath(*sceneArgument, context, workingDirectory), scene);
				if (!loadResult.Succeeded()) {
					return { std::move(loadResult) };
				}

				request.SceneTarget = scene.get();
				request.ScriptScene = options.HasFlag("--include-scripts") ? scene.get() : nullptr;
				request.IncludeScripts = options.HasFlag("--include-scripts");
			}

			auto result = m_Operations->Validate(request);
			result.SetPayloadValue("project_root", context.RootPath.generic_string());
			if (scene) {
				result.SetPayloadValue("scene_path", ResolveScenePath(*options.GetValue("--scene"), context, workingDirectory).generic_string());
			}
			return { std::move(result) };
		}

		return { MakeUsageError("Unknown command", command + (subcommand.empty() ? std::string() : " " + subcommand)) };
	}
}
