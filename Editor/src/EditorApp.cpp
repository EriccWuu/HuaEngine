#include "enginepch.h"
#include "EditorLayer.h"
#include "Launch/EditorLaunch.h"

#include "HuaEngine/Core/HostLaunch.h"

#include "Workbench/EcsHostSmoke.h"

namespace HE {
	class EditorApp : public Application {
	public:
		EditorApp(
			CommandLineArguments args,
			const Editor::EditorLaunchOptions& launchOptions,
			std::shared_ptr<HostSmoke::Session> smoke = {})
			: Application(ApplicationSpecification{
				.Name = "HuaEditor",
				.EnableGuiLayer = true,
				.CommandLineArgs = args
			}) {
			EditorLayerSpecification layerSpecification;
			layerSpecification.StartupProjectPath = launchOptions.ProjectPath;
			layerSpecification.StartupScenePath = launchOptions.ScenePath;
			layerSpecification.Smoke = std::move(smoke);
			PushLayer(new EditorLayer(layerSpecification));
		}

		~EditorApp() {

		}
	};

	HE::Application* HE::CreateApplication(CommandLineArguments args) {
		const auto launchOptions = Editor::ParseEditorLaunchOptions(args);
		if (launchOptions.Target == Editor::EditorLaunchTarget::ProjectHub) {
			if (HostLaunch::LaunchSibling("ProjectHub.exe")) {
				HE_CORE_INFO("[Editor] Delegated no-project startup to ProjectHub.exe");
				return nullptr;
			}

			HE_CORE_ERROR(
				"[Editor] Failed to launch standalone ProjectHub from '{}'",
				HostLaunch::ResolveSiblingExecutable("ProjectHub.exe").generic_string());
		}

		return new EditorApp(args, launchOptions);
	}
}

int main(int count, char** values) {
	return HE::HostSmoke::Run({ count, values }, "Editor", [](HE::CommandLineArguments args, std::shared_ptr<HE::HostSmoke::Session> smoke) {
		if (!smoke) return std::unique_ptr<HE::Application>(HE::CreateApplication(args));

		HE::Editor::EditorLaunchOptions launchOptions;
		launchOptions.Target = HE::Editor::EditorLaunchTarget::Workbench;
		return std::unique_ptr<HE::Application>(std::make_unique<HE::EditorApp>(args, launchOptions, std::move(smoke)));
	});
}
