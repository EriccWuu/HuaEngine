#pragma once

#include <string>

#include "HeadlessCommandRunner.h"

namespace HE::Headless {
	[[nodiscard]] std::string RenderJson(const HeadlessCommandResponse& response);
	[[nodiscard]] int ExitCodeFor(const ResultEnvelope& result);
}
