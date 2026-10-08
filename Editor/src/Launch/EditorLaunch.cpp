#include "EditorLaunch.h"

#include <string_view>

namespace HE::Editor {
	EditorLaunchOptions ParseEditorLaunchOptions(CommandLineArguments args) {
		EditorLaunchOptions options;
		options.Target = args.Count <= 1
			? EditorLaunchTarget::ProjectHub
			: EditorLaunchTarget::Workbench;

		for (int index = 1; index < args.Count; ++index) {
			const char* token = args[index];
			if (token == nullptr) {
				continue;
			}

			const std::string_view argument(token);
			auto tryConsumePath = [&](std::filesystem::path& outPath) {
				if (index + 1 >= args.Count || args[index + 1] == nullptr) {
					return false;
				}

				outPath = args[++index];
				return true;
			};

			if (argument == "--project") {
				tryConsumePath(options.ProjectPath);
			}
			else if (argument == "--scene") {
				tryConsumePath(options.ScenePath);
			}
		}

		return options;
	}
}
