#include "enginepch.h"
#include "CLIApplication.h"

namespace HE::CLI {
	CLIApplication::CLIApplication()
		: Application(ApplicationSpecification{
			.Name = "HuaEngineCLI",
			.EnableWindow = false,
			.EnableGuiLayer = false
		})
	{
	}
}
