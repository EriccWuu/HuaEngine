#include "enginepch.h"
#include "HeadlessApplication.h"

namespace HE::Headless {
	HeadlessApplication::HeadlessApplication()
		: Application(ApplicationSpecification{
			.Name = "HuaEngineHeadless",
			.EnableWindow = false,
			.EnableGuiLayer = false
		})
	{
	}
}
