#pragma once

#include "HuaEngine/Application.h"

namespace HE::Headless {
	class HeadlessApplication final : public Application {
	public:
		HeadlessApplication();
		~HeadlessApplication() override = default;
	};
}
