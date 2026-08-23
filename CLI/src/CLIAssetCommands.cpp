#include "enginepch.h"
#include "CLIAssetCommands.h"

#include "HuaEngine/Project/ProjectContext.h"

namespace HE::CLI {
	std::optional<BuiltinMeshPrimitive> ParseBuiltinMeshPrimitive(std::string_view primitive) {
		if (primitive == "quad") {
			return BuiltinMeshPrimitive::Quad;
		}
		if (primitive == "cube") {
			return BuiltinMeshPrimitive::Cube;
		}
		if (primitive == "sphere") {
			return BuiltinMeshPrimitive::Sphere;
		}

		return std::nullopt;
	}

	std::optional<AssetKind> ParseAssetKind(std::string_view kind) {
		if (kind == "mesh") {
			return AssetKind::Mesh;
		}
		if (kind == "material") {
			return AssetKind::Material;
		}
		if (kind == "texture2d") {
			return AssetKind::Texture2D;
		}
		if (kind == "shader") {
			return AssetKind::Shader;
		}

		return std::nullopt;
	}

	namespace {
		void AddAssetRecordPayload(ResultEnvelope& result, size_t index, const AssetRecord& record) {
			const auto prefix = "asset_" + std::to_string(index) + "_";
			result.SetPayloadValue(prefix + "guid", record.Guid);
			result.SetPayloadValue(prefix + "id", record.AssetId);
			result.SetPayloadValue(prefix + "kind", std::string(ToString(record.Kind)));
			result.SetPayloadValue(prefix + "source", std::string(ToString(record.Source)));
			result.SetPayloadValue(prefix + "import_state", std::string(ToString(record.ImportState)));
		}

		void AddResolvedAssetPayload(ResultEnvelope& result, const AssetRecord& record) {
			result.SetPayloadValue("asset_guid", record.Guid);
			result.SetPayloadValue("asset_id", record.AssetId);
			result.SetPayloadValue("asset_kind", std::string(ToString(record.Kind)));
			result.SetPayloadValue("asset_source", std::string(ToString(record.Source)));
			result.SetPayloadValue("asset_handle", std::to_string(record.Handle));
			result.SetPayloadValue("import_state", std::string(ToString(record.ImportState)));
		}
	}

	CLICommandResponse RunAssetCommand(
		const CLICommandDefinition& command,
		const CLIParsedOptions& options,
		CLICommandContext& context) {
		if (command.Path == std::vector<std::string>{ "asset", "register-default-mesh" }) {
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

			ProjectContext projectContext;
			ResultEnvelope resolveResult;
			if (!ResolveProjectContext(context.Operations, options.GetValue("--project"), context.WorkingDirectory, projectContext, resolveResult)) {
				return { std::move(resolveResult) };
			}

			auto result = context.Operations.CreateBuiltinMeshAsset(projectContext, *assetId, *builtinPrimitive, meshName);
			return { std::move(result) };
		}

		if (command.Path == std::vector<std::string>{ "asset", "manifest", "init" }) {
			ProjectContext projectContext;
			ResultEnvelope resolveResult;
			if (!ResolveProjectContext(context.Operations, options.GetValue("--project"), context.WorkingDirectory, projectContext, resolveResult)) {
				return { std::move(resolveResult) };
			}

			return { context.Operations.InitializeAssetManifest(projectContext) };
		}

		if (command.Path == std::vector<std::string>{ "asset", "import" }) {
			const auto assetId = options.GetValue("--asset-id");
			if (!assetId.has_value() || assetId->empty()) {
				return { MakeUsageError("asset import requires --asset-id") };
			}

			const auto kindArgument = options.GetValue("--kind");
			if (!kindArgument.has_value() || kindArgument->empty()) {
				return { MakeUsageError("asset import requires --kind") };
			}

			const auto kind = ParseAssetKind(*kindArgument);
			if (!kind.has_value()) {
				return { MakeUsageError("Unsupported asset kind", *kindArgument) };
			}

			ProjectContext projectContext;
			ResultEnvelope resolveResult;
			if (!ResolveProjectContext(context.Operations, options.GetValue("--project"), context.WorkingDirectory, projectContext, resolveResult)) {
				return { std::move(resolveResult) };
			}

			AssetGuid importedGuid;
			auto result = context.Operations.ImportAsset(projectContext, *assetId, *kind, &importedGuid);
			if (!importedGuid.empty()) {
				result.SetPayloadValue("asset_guid", importedGuid);
			}
			return { std::move(result) };
		}

		if (command.Path == std::vector<std::string>{ "asset", "list" }) {
			ProjectContext projectContext;
			ResultEnvelope resolveResult;
			if (!ResolveProjectContext(context.Operations, options.GetValue("--project"), context.WorkingDirectory, projectContext, resolveResult)) {
				return { std::move(resolveResult) };
			}

			std::vector<AssetRecord> records;
			auto result = context.Operations.ListAssets(projectContext, records);
			for (size_t index = 0; index < records.size(); ++index) {
				AddAssetRecordPayload(result, index, records[index]);
			}
			return { std::move(result) };
		}

		if (command.Path == std::vector<std::string>{ "asset", "resolve" }) {
			const auto guid = options.GetValue("--guid");
			const auto assetId = options.GetValue("--asset-id");
			const bool hasGuid = guid.has_value() && !guid->empty();
			const bool hasAssetId = assetId.has_value() && !assetId->empty();
			if (!hasGuid && !hasAssetId) {
				return { MakeUsageError(command, "asset resolve requires --guid or --asset-id") };
			}
			if (hasGuid && hasAssetId) {
				return { MakeUsageError(command, "asset resolve accepts either --guid or --asset-id, not both") };
			}

			ProjectContext projectContext;
			ResultEnvelope resolveResult;
			if (!ResolveProjectContext(context.Operations, options.GetValue("--project"), context.WorkingDirectory, projectContext, resolveResult)) {
				return { std::move(resolveResult) };
			}

			AssetRecord record;
			auto result = hasGuid
				? context.Operations.ResolveAssetByGuid(projectContext, *guid, record)
				: context.Operations.ResolveAsset(projectContext, *assetId, record);
			if (result.Succeeded()) {
				AddResolvedAssetPayload(result, record);
			}
			return { std::move(result) };
		}

		if (command.Path == std::vector<std::string>{ "asset", "validate" }) {
			ProjectContext projectContext;
			ResultEnvelope resolveResult;
			if (!ResolveProjectContext(context.Operations, options.GetValue("--path"), context.WorkingDirectory, projectContext, resolveResult)) {
				return { std::move(resolveResult) };
			}

			return { context.Operations.ValidateAssets(projectContext) };
		}

		return { MakeUsageError(command, "Unknown command") };
	}
}
