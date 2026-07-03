#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "CLICommandCatalog.h"
#include "CLICommandContext.h"
#include "CLICommandRunner.h"
#include "CLIOptionParser.h"
#include "HuaEngine/Application/ApplicationOperations.h"

namespace HE::CLI {
	[[nodiscard]] std::string SanitizeFileStem(std::string_view value);
	[[nodiscard]] std::optional<uint32_t> ParseEntityId(std::string_view value);
	[[nodiscard]] std::optional<SceneComponentKind> ParseSceneComponentKind(std::string_view value);

	[[nodiscard]] CLICommandResponse RunSceneCommand(
		const CLICommandDefinition& command,
		const CLIParsedOptions& options,
		CLICommandContext& context);
}
