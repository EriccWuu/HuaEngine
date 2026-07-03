#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "CLIApplication.h"
#include "CLICommandCatalog.h"
#include "CLICommandRunner.h"
#include "CLIJsonWriter.h"
#include "HuaEngine/Core/Log.h"

namespace {
	void Expect(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << message << std::endl;
			std::exit(1);
		}
	}

	bool StartsWith(std::string_view value, std::string_view prefix) {
		return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
	}
}

int main() {
	HE::Log::Init({ .EnableConsoleOutput = false });

	const auto tempRoot = std::filesystem::temp_directory_path() / "huaengine_cli_host_smoke";
	std::error_code errorCode;
	std::filesystem::remove_all(tempRoot, errorCode);
	std::filesystem::create_directories(tempRoot, errorCode);
	Expect(!errorCode, "Failed to create temporary smoke directory");

	HE::CLI::CLIApplication application;
	application.Start();

	HE::CLI::CLICommandCatalog catalog;
	Expect(!catalog.Commands().empty(), "CLI command catalog should not be empty");
	for (const auto& command : catalog.Commands()) {
		Expect(!command.Summary.empty(), "Catalog command summary should not be empty");
		Expect(!command.Usage.empty(), "Catalog command usage should not be empty");

		if (!StartsWith(command.FormalOperation, "cli.")) {
			Expect(
				application.GetOperations().Supports(command.FormalOperation),
				"Catalog command formal operation should be registered: " + command.FormalOperation);
		}
	}

	HE::CLI::CommandRunner runner(application.GetOperations());

	auto helpResponse = runner.Run({ "help" }, tempRoot);
	Expect(helpResponse.Result.Succeeded(), "help should succeed");
	Expect(!helpResponse.Result.Details.empty(), "help should emit catalog-backed details");
	bool hasCatalogHelpDetail = false;
	for (const auto& detail : helpResponse.Result.Details) {
		if (detail.Code == "cli.help.command") {
			hasCatalogHelpDetail = true;
			break;
		}
	}
	Expect(hasCatalogHelpDetail, "help should include cli.help.command details from the catalog");

	auto opsResponse = runner.Run({ "ops", "list" }, tempRoot);
	Expect(opsResponse.Result.Succeeded(), "ops list should succeed");
	Expect(!opsResponse.Operations.empty(), "ops list should expose the operation registry");

	auto projectResponse = runner.Run({ "project", "init", "--root", tempRoot.string(), "--name", "CLISmoke" }, tempRoot);
	Expect(projectResponse.Result.Succeeded(), "project init should succeed");

	auto sceneResponse = runner.Run({ "scene", "create", "--project", tempRoot.string(), "--name", "SmokeScene" }, tempRoot);
	Expect(sceneResponse.Result.Succeeded(), "scene create should succeed");
	Expect(std::filesystem::exists(tempRoot / "Scenes" / "smokescene.scene"), "scene create should persist the scene file");

	auto assetResponse = runner.Run({
		"asset", "register-default-mesh",
		"--project", tempRoot.string(),
		"--asset-id", "primitives/quad.mesh",
		"--primitive", "quad"
	}, tempRoot);
	Expect(assetResponse.Result.Succeeded(), "asset register-default-mesh should succeed");
	Expect(std::filesystem::exists(tempRoot / "Assets" / "primitives" / "quad.mesh"), "default mesh registration should persist the mesh asset");

	auto scriptResponse = runner.Run({
		"script", "status",
		"--project", tempRoot.string(),
		"--scene", "smokescene.scene"
	}, tempRoot);
	Expect(scriptResponse.Result.Succeeded(), "script status should succeed for a scene without bindings");

	auto validationResponse = runner.Run({
		"validation", "run",
		"--path", tempRoot.string(),
		"--scene", "smokescene.scene",
		"--include-assets",
		"--include-scripts"
	}, tempRoot);
	Expect(validationResponse.Result.Succeeded(), "validation run should succeed for the smoke workflow");

	const auto renderedJson = HE::CLI::RenderJson(validationResponse);
	Expect(renderedJson.find("\"host\":\"huaengine-cli\"") != std::string::npos, "Rendered JSON should identify the cli host");
	Expect(renderedJson.find("\"operation\":\"validation.validate\"") != std::string::npos, "Rendered JSON should preserve the formal operation id");

	std::filesystem::remove_all(tempRoot, errorCode);
	std::cout << "CLIHostSmoke passed" << std::endl;
	return 0;
}
