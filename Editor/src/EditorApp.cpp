#include "enginepch.h"
#include "EditorLayer.h"

#include <filesystem>
#include <string_view>

#include "Workbench/EcsHostSmoke.h"

namespace HE {
	namespace {
		struct EditorLaunchOptions {
			std::filesystem::path ProjectPath;
			std::filesystem::path ScenePath;
		};

		EditorLaunchOptions ParseEditorLaunchOptions(CommandLineArguments args) {
			EditorLaunchOptions options;

			for (int index = 1; index < args.Count; ++index) {
				const char* token = args[index];
				if (token == nullptr) {
					continue;
				}

				const std::string_view argument(token);
				auto tryConsumeValue = [&](std::filesystem::path& outPath) {
					if (index + 1 >= args.Count || args[index + 1] == nullptr) {
						return false;
					}

					outPath = args[++index];
					return true;
				};

				if (argument == "--project") {
					tryConsumeValue(options.ProjectPath);
				}
				else if (argument == "--scene") {
					tryConsumeValue(options.ScenePath);
				}
			}

			return options;
		}
	}

	class EditorApp : public Application {
	public:
		explicit EditorApp(CommandLineArguments args, std::shared_ptr<HostSmoke::Session> smoke = {})
			: Application(ApplicationSpecification{
				.Name = "HuaEditor",
				.EnableGuiLayer = true,
				.CommandLineArgs = args
			}) {
			const auto launchOptions = smoke ? EditorLaunchOptions{} : ParseEditorLaunchOptions(args);
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
		return new EditorApp(args);
	}
}

int main(int count, char** values) {
    return HE::HostSmoke::Run({count, values}, "Editor", [](HE::CommandLineArguments args, std::shared_ptr<HE::HostSmoke::Session> smoke) {
        return std::make_unique<HE::EditorApp>(args, std::move(smoke));
    });
}
