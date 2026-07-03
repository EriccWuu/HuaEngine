#pragma once

#include <string>

#include "CLICommandRunner.h"

namespace HE::CLI {
	[[nodiscard]] std::string RenderJson(const CLICommandResponse& response);
	[[nodiscard]] int ExitCodeFor(const ResultEnvelope& result);
}
