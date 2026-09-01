#include "enginepch.h"
#include "EditorLayer.h"
#include "Launch/EditorLaunch.h"

#include "HuaEngine/Core/HostLaunch.h"

// Entry Point - Must be included in main application file only
#include "HuaEngine/EntryPoint.h"

namespace HE {
	class EditorApp : public Application {
	public:
		EditorApp(CommandLineArguments args, const Editor::EditorLaunchOptions& launchOptions)
			: Application(ApplicationSpecification{
				.Name = "HuaEditor",
				.EnableGuiLayer = true,
				.CommandLineArgs = args
			}) {
			EditorLayerSpecification layerSpecification;
			layerSpecification.StartupProjectPath = launchOptions.ProjectPath;
			layerSpecification.StartupScenePath = launchOptions.ScenePath;
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
