#pragma once

#include "HuaEngine/Application.h"

namespace HE::CLI {
	class CLIApplication final : public Application {
	public:
		CLIApplication();
		~CLIApplication() override = default;
	};
}
