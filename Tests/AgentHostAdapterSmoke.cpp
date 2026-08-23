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
	Require(assetCreate.Result.Payload.find("asset_guid") != assetCreate.Result.Payload.end(), "Expected asset create payload to include asset guid");
	Require(!assetCreate.Result.Payload.at("asset_guid").empty(), "Expected asset create payload to include asset guid");
	Require(assetCreate.Result.Payload.find("asset_handle") != assetCreate.Result.Payload.end(), "Expected runtime handle payload");
	Require(assetCreate.Result.Payload.at("asset_handle") != "0", "Expected runtime handle payload");

	auto assetInitialize = adapter.Invoke({
		.Operation = "asset.initialize",
		.Arguments = { { "project_path", (smokeRoot / "Project").string() } },
		.WorkingDirectory = smokeRoot
	});
	Require(assetInitialize.Result.Succeeded(), "Expected asset.initialize to succeed through the agent adapter");
	Require(assetInitialize.Result.Operation == "asset.initialize", "Expected asset.initialize to preserve its operation id");

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

	auto validation = adapter.Invoke({
		.Operation = "validation.validate",
		.Arguments = {
			{ "path", (smokeRoot / "Project").string() },
			{ "scene", "agentscene.scene" },
			{ "include_assets", "true" }
		},
		.WorkingDirectory = smokeRoot
	});
	if (validation.Result.Failed()) {
		std::cerr << "[AgentHostAdapterSmoke] Validation summary: " << validation.Result.Summary << std::endl;
		for (const auto& detail : validation.Result.Details) {
			std::cerr << "[AgentHostAdapterSmoke] " << detail.Code << ": " << detail.Message;
			if (!detail.Context.empty()) {
				std::cerr << " (" << detail.Context << ")";
			}
			std::cerr << std::endl;
		}
	}
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
