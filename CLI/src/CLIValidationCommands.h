#pragma once

#include "CLICommandCatalog.h"
#include "CLICommandContext.h"
#include "CLICommandRunner.h"
#include "CLIOptionParser.h"

namespace HE::CLI {
	[[nodiscard]] CLICommandResponse RunValidationCommand(
		const CLICommandDefinition& command,
		const CLIParsedOptions& options,
		CLICommandContext& context);
}
