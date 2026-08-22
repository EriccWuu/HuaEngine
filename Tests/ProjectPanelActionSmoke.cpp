#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "Panels/ProjectPanel.h"

namespace {
	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[ProjectPanelActionSmoke] " << message << std::endl;
			std::exit(1);
		}
	}
}

int main() {
	const std::filesystem::path filePath = "Assets/Meshes/Quad.mesh";
	const auto fileAction = HE::MakeProjectReimportAction(filePath, false);
	Require(fileAction.Type == HE::ProjectPanelActionType::ReimportPath, "Expected file reimport action");
	Require(fileAction.Path == filePath, "Expected file target path");

	const auto directoryAction = HE::MakeProjectReimportAction("Assets/Meshes", false);
	Require(directoryAction.Type == HE::ProjectPanelActionType::ReimportPath, "Expected directory reimport action");

	const auto allAction = HE::MakeProjectReimportAction({}, true);
	Require(allAction.Type == HE::ProjectPanelActionType::ReimportAll, "Expected reimport all action");
	Require(allAction.Path.empty(), "Expected reimport all to defer asset root resolution to the editor");

	std::cout << "ProjectPanelActionSmoke passed" << std::endl;
	return 0;
}
