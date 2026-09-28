#include "enginepch.h"
#include "ProjectHubLayer.h"

#include "Workbench/EcsHostSmoke.h"

namespace HE {
	class ProjectHubApp : public Application {
	public:
		explicit ProjectHubApp(CommandLineArguments args, std::shared_ptr<HostSmoke::Session> smoke = {})
			: Application(ApplicationSpecification{
				.Name = "HuaEngine Project Hub",
				.EnableGuiLayer = true,
				.WindowWidth = 1024,
				.WindowHeight = 646,
				.CommandLineArgs = args
			}) {
			PushLayer(new ProjectHubLayer(std::move(smoke)));
		}
	};

	HE::Application* HE::CreateApplication(CommandLineArguments args) {
		return new ProjectHubApp(args);
	}
}

int main(int count, char** values) {
    return HE::HostSmoke::Run({count, values}, "ProjectHub", [](HE::CommandLineArguments args, std::shared_ptr<HE::HostSmoke::Session> smoke) {
        return std::make_unique<HE::ProjectHubApp>(args, std::move(smoke));
    });
}
