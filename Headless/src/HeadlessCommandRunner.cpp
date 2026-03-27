#include "enginepch.h"
#include "HeadlessCommandRunner.h"

#include <algorithm>
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
		result.AddDetail({ DiagnosticSeverity::Info, "cli.usage.example", "Supported commands include: ops list, project init, project status, scene create, scene validate, asset register-default-mesh, asset validate, script status, script initialize, script update, script shutdown, validation run", {} });
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
}

namespace HE::Headless {
	CommandRunner::CommandRunner(ApplicationOperations& operations)
		: m_Operations(&operations)
	{
	}

	HeadlessCommandResponse CommandRunner::Run(
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
		const std::span<const std::string> optionTokens(arguments.data() + std::min<size_t>(2, arguments.size()), arguments.size() > 2 ? arguments.size() - 2 : 0);

		if (command == "help") {
			auto result = ResultEnvelope::Success("cli.help", "command_line", "Headless command help");
			result.AddDetail({ DiagnosticSeverity::Info, "cli.help.summary", "Use 'ops list' to inspect the formal operation registry", {} });
			result.AddDetail({ DiagnosticSeverity::Info, "cli.help.commands", "Supported commands include: project init/status, scene create/validate, asset register-default-mesh/validate, script status/initialize/update/shutdown, validation run", {} });
			return { std::move(result) };
		}

		if (command == "ops" && subcommand == "list") {
			if (!optionTokens.empty()) {
				return { MakeUsageError("ops list does not accept options") };
			}

			auto result = ResultEnvelope::Success("cli.ops_list", "operation_registry", "Formal operation registry listed");
			result.SetPayloadValue("operation_count", std::to_string(m_Operations->GetRegistry().Size()));
			return { std::move(result), m_Operations->GetRegistry().List() };
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
