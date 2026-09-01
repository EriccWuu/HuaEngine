#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "Assets/AssetCreationRegistry.h"
#include "HuaEngine/Asset/AssetRegistry.h"

namespace HE::Editor {
	using AssetCreateHandler = std::function<ResultEnvelope(const std::filesystem::path&, AssetGuid*)>;
	using AssetRenameHandler = std::function<ResultEnvelope(const AssetGuid&, std::string_view, AssetRecord*)>;

	struct AssetWorkspaceMutation {
		ResultEnvelope Result;
		AssetGuid Guid;
		std::filesystem::path OldPath;
		std::filesystem::path NewPath;
		bool BeginRename = false;
	};

	class AssetWorkspaceController {
	public:
		[[nodiscard]] ResultEnvelope RegisterCreator(std::string typeId, AssetCreateHandler handler);
		void SetRenameHandler(AssetRenameHandler handler) { m_RenameHandler = std::move(handler); }
		[[nodiscard]] AssetWorkspaceMutation CreateAsset(
			const AssetCreationRegistry& registry,
			std::string_view typeId,
			const std::filesystem::path& targetDirectory) const;
		[[nodiscard]] AssetWorkspaceMutation RenameAsset(const AssetGuid& guid, std::string_view newBaseName) const;

	private:
		std::unordered_map<std::string, AssetCreateHandler> m_CreateHandlers;
		AssetRenameHandler m_RenameHandler;
	};
}
