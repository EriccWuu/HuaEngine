#pragma once

#include <optional>
#include <string_view>

#include "CLICommandCatalog.h"
#include "CLICommandContext.h"
#include "CLICommandRunner.h"
#include "CLIOptionParser.h"
#include "HuaEngine/Asset/AssetRegistry.h"

namespace HE::CLI {
	[[nodiscard]] std::optional<BuiltinMeshPrimitive> ParseBuiltinMeshPrimitive(std::string_view primitive);
	[[nodiscard]] std::optional<AssetKind> ParseAssetKind(std::string_view kind);

	[[nodiscard]] CLICommandResponse RunAssetCommand(
		const CLICommandDefinition& command,
		const CLIParsedOptions& options,
		CLICommandContext& context);
}
