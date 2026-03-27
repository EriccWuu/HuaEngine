#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "HuaEngine.h"

namespace {
	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[AgentHostAdapterSmoke] " << message << std::endl;
			std::exit(1);
		}
	}

	HE::ApplicationSpecification MakeApplicationSpecification() {
		HE::ApplicationSpecification specification;
		specification.Name = "AgentHostAdapterSmoke";
		specification.EnableGuiLayer = false;
		specification.EnableWindow = false;
		return specification;
	}

	class SmokeApplication final : public HE::Application {
	public:
		SmokeApplication()
			: HE::Application(MakeApplicationSpecification()) {}
	};
}

int main() {
	HE::Log::Init({ .EnableConsoleOutput = false });

	SmokeApplication application;
	application.Start();

	HE::AgentHostAdapter adapter(application.GetOperations());
	const auto smokeRoot = std::filesystem::temp_directory_path() / "HuaEngineAgentHostAdapterSmoke";
	std::error_code errorCode;
	std::filesystem::remove_all(smokeRoot, errorCode);

	auto opsList = adapter.Invoke({ .Operation = "ops.list", .WorkingDirectory = smokeRoot });
	Require(opsList.Result.Succeeded(), "Expected ops.list to succeed");
	Require(!opsList.Operations.empty(), "Expected ops.list to expose registered operations");

	auto projectInit = adapter.Invoke({
		.Operation = "project.initialize",
		.Arguments = { { "root", (smokeRoot / "Project").string() }, { "name", "AgentProject" } },
		.WorkingDirectory = smokeRoot
	});
	Require(projectInit.Result.Succeeded(), "Expected project.initialize to succeed");

	auto assetCreate = adapter.Invoke({
		.Operation = "asset.create_builtin_mesh",
		.Arguments = {
			{ "project_path", (smokeRoot / "Project").string() },
			{ "asset_id", "Meshes/AgentQuad.mesh" },
			{ "primitive", "quad" }
		},
		.WorkingDirectory = smokeRoot
	});
	Require(assetCreate.Result.Succeeded(), "Expected asset.create_builtin_mesh to succeed");

	auto sceneCreate = adapter.Invoke({
		.Operation = "scene.create",
		.Arguments = {
			{ "project_path", (smokeRoot / "Project").string() },
			{ "scene_name", "AgentScene" },
			{ "output", "agentscene.scene" }
		},
		.WorkingDirectory = smokeRoot
	});
	Require(sceneCreate.Result.Succeeded(), "Expected scene.create to succeed");
	Require(sceneCreate.Result.Operation == "scene.create", "Expected adapter to preserve scene.create operation id");
	Require(sceneCreate.Result.Payload.contains("scene_path"), "Expected scene.create to preserve scene_path payload");

	auto scriptStatus = adapter.Invoke({
		.Operation = "script.status",
		.Arguments = {
			{ "project_path", (smokeRoot / "Project").string() },
			{ "scene", "agentscene.scene" }
		},
		.WorkingDirectory = smokeRoot
	});
	Require(scriptStatus.Result.Succeeded(), "Expected script.status to succeed");

	auto validation = adapter.Invoke({
		.Operation = "validation.validate",
		.Arguments = {
			{ "path", (smokeRoot / "Project").string() },
			{ "scene", "agentscene.scene" },
			{ "include_assets", "true" },
			{ "include_scripts", "true" }
		},
		.WorkingDirectory = smokeRoot
	});
	Require(validation.Result.Succeeded(), "Expected validation.validate to succeed");
	Require(validation.Result.Operation == "validation.validate", "Expected adapter to preserve stable operation ids");

	auto unsupported = adapter.Invoke({ .Operation = "unsupported.operation", .WorkingDirectory = smokeRoot });
	Require(unsupported.Result.Failed(), "Expected unsupported operations to fail through the shared result envelope");
	Require(unsupported.Result.Operation == "unsupported.operation", "Expected unsupported operation id to remain stable");
	Require(!unsupported.Result.Details.empty(), "Expected unsupported operations to surface diagnostic details");

	std::filesystem::remove_all(smokeRoot, errorCode);
	Require(!errorCode, "Expected temporary cleanup to succeed");
	std::cout << "AgentHostAdapterSmoke passed" << std::endl;
	return 0;
}
