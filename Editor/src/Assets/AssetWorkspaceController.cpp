#include "enginepch.h"
#include "Assets/AssetWorkspaceController.h"

#include "Panels/ProjectAssetNaming.h"

namespace {
	std::filesystem::path GetPayloadPath(const HE::ResultEnvelope& result, std::string_view key) {
		const auto found = result.Payload.find(std::string(key));
		return found != result.Payload.end() ? std::filesystem::path(found->second) : std::filesystem::path{};
	}
}

namespace HE::Editor {
	ResultEnvelope AssetWorkspaceController::RegisterCreator(std::string typeId, AssetCreateHandler handler) {
		if (typeId.empty() || !handler) {
			return ResultEnvelope::Failure("asset.workspace.creator.register", typeId, "Asset creation handler is incomplete");
		}
		if (m_CreateHandlers.contains(typeId)) {
			return ResultEnvelope::Failure("asset.workspace.creator.register", typeId, "Asset creation handler is already registered");
		}

		m_CreateHandlers.emplace(typeId, std::move(handler));
		return ResultEnvelope::Success("asset.workspace.creator.register", typeId, "Asset creation handler registered");
	}

	AssetWorkspaceMutation AssetWorkspaceController::CreateAsset(
		const AssetCreationRegistry& registry,
		std::string_view typeId,
		const std::filesystem::path& targetDirectory) const {
		const auto* descriptor = registry.Find(typeId);
		const auto handler = m_CreateHandlers.find(std::string(typeId));
		if (!descriptor || handler == m_CreateHandlers.end()) {
			return { .Result = ResultEnvelope::Failure("asset.workspace.create", std::string(typeId), "Asset creation type is unavailable") };
		}

		const auto assetPath = GenerateUniqueAssetPath(targetDirectory, descriptor->DefaultBaseName, descriptor->Extension);
		AssetGuid guid;
		auto result = handler->second(assetPath, &guid);
		if (!result.Succeeded()) {
			return { .Result = std::move(result) };
		}
		if (guid.empty()) {
			const auto guidPayload = result.Payload.find("asset_guid");
			if (guidPayload != result.Payload.end()) guid = guidPayload->second;
		}
		return {
			.Result = std::move(result),
			.Guid = std::move(guid),
			.NewPath = assetPath,
			.BeginRename = true
		};
	}

	AssetWorkspaceMutation AssetWorkspaceController::RenameAsset(const AssetGuid& guid, std::string_view newBaseName) const {
		if (auto validation = ValidateAssetBaseName(newBaseName); !validation.Succeeded()) {
			return { .Result = std::move(validation), .Guid = guid };
		}
		if (!m_RenameHandler) {
			return { .Result = ResultEnvelope::Failure("asset.workspace.rename", guid, "Asset rename handler is unavailable"), .Guid = guid };
		}

		AssetRecord record;
		auto result = m_RenameHandler(guid, newBaseName, &record);
		if (!result.Succeeded()) {
			return { .Result = std::move(result), .Guid = guid };
		}
		const auto oldPath = GetPayloadPath(result, "old_asset_path");
		auto newPath = record.AbsolutePath;
		if (newPath.empty()) newPath = GetPayloadPath(result, "asset_path");
		return {
			.Result = std::move(result),
			.Guid = guid,
			.OldPath = oldPath,
			.NewPath = newPath
		};
	}

	AssetWorkspaceMutation AssetWorkspaceController::DeleteAsset(const AssetGuid& guid) const {
		if (guid.empty()) {
			return { .Result = ResultEnvelope::Failure("asset.workspace.delete", guid, "Asset identity is required") };
		}
		if (!m_DeleteHandler) {
			return { .Result = ResultEnvelope::Failure("asset.workspace.delete", guid, "Asset delete handler is unavailable"), .Guid = guid };
		}

		return {
			.Result = m_DeleteHandler(guid),
			.Guid = guid
		};
	}
}
