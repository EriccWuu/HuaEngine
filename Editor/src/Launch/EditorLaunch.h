#pragma once

#include <filesystem>

#include "HuaEngine/Application.h"

namespace HE::Editor {
	enum class EditorLaunchTarget {
		ProjectHub,
		Workbench
	};

	struct EditorLaunchOptions {
		EditorLaunchTarget Target = EditorLaunchTarget::ProjectHub;
		std::filesystem::path ProjectPath;
		std::filesystem::path ScenePath;
	};

	[[nodiscard]] EditorLaunchOptions ParseEditorLaunchOptions(CommandLineArguments args);
}
