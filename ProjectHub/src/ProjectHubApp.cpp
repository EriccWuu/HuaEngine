#include "enginepch.h"
#include "ProjectHubLayer.h"

#include "HuaEngine/EntryPoint.h"

namespace HE {
	class ProjectHubApp : public Application {
	public:
		explicit ProjectHubApp(CommandLineArguments args)
			: Application(ApplicationSpecification{
				.Name = "HuaEngine Project Hub",
				.EnableGuiLayer = true,
				.WindowWidth = 1024,
				.WindowHeight = 646,
				.CommandLineArgs = args
			}) {
			PushLayer(new ProjectHubLayer());
		}
	};

	HE::Application* HE::CreateApplication(CommandLineArguments args) {
		return new ProjectHubApp(args);
	}
}
