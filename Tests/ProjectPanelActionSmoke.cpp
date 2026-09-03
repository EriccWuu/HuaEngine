#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "Assets/AssetCreationRegistry.h"
#include "Assets/AssetWorkspaceController.h"
#include "Panels/ConsolePanel.h"
#include "Panels/ProjectPanel.h"
#include "Panels/ProjectAssetNaming.h"

namespace {
	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[ProjectPanelActionSmoke] " << message << std::endl;
			std::exit(1);
		}
	}
}

int main() {
	const std::vector<HE::LogSink::LogLine> logLines{
		{ spdlog::level::info, "First log line" },
		{ spdlog::level::err, "Second log line" }
	};
	Require(HE::BuildConsoleLogText(logLines) == "First log line\nSecond log line", "Expected console copy text to preserve complete log lines");

	const auto smokeRoot = std::filesystem::temp_directory_path() / "HuaEngineProjectPanelActionSmoke";
	std::error_code errorCode;
	std::filesystem::remove_all(smokeRoot, errorCode);
	const auto assetDirectory = smokeRoot / "Assets" / "Scenes";
	std::filesystem::create_directories(assetDirectory, errorCode);
	Require(!errorCode, "Expected naming fixture directory");

	HE::Editor::AssetCreationRegistry creationRegistry;
	Require(creationRegistry.Register({ "scene", HE::AssetKind::Scene, "Scene", "New Scene", ".scene" }).Succeeded(), "Expected scene creation registration");
	Require(creationRegistry.Find("scene") != nullptr, "Expected scene creation lookup");
	Require(!creationRegistry.Register({ "scene", HE::AssetKind::Scene, "Duplicate", "New Scene", ".scene" }).Succeeded(), "Expected duplicate creation type rejection");

	const auto firstScenePath = HE::Editor::GenerateUniqueAssetPath(assetDirectory, "New Scene", ".scene");
	Require(firstScenePath.filename() == "New Scene.scene", "Expected default scene asset name");
	std::ofstream(firstScenePath).put('\n');
	const auto secondScenePath = HE::Editor::GenerateUniqueAssetPath(assetDirectory, "New Scene", ".scene");
	Require(secondScenePath.filename() == "New Scene 1.scene", "Expected incremented scene asset name");
	Require(HE::Editor::ValidateAssetBaseName("Renamed").Succeeded(), "Expected valid asset base name");
	Require(HE::Editor::ValidateAssetBaseName("../Renamed").Failed(), "Expected path separator rejection");
	Require(HE::Editor::ValidateAssetBaseName("CON").Failed(), "Expected reserved asset name rejection");

	HE::Editor::AssetWorkspaceController workspaceController;
	std::filesystem::path handledCreatePath;
	Require(workspaceController.RegisterCreator("scene", [&](const std::filesystem::path& path, HE::AssetGuid* outGuid) {
		handledCreatePath = path;
		if (outGuid) *outGuid = "created-guid";
		auto result = HE::ResultEnvelope::Success("asset.scene.create", path.generic_string(), "Created");
		result.SetPayloadValue("asset_guid", "created-guid");
		result.SetPayloadValue("asset_path", path.generic_string());
		return result;
	}).Succeeded(), "Expected scene creation handler registration");
	const auto createMutation = workspaceController.CreateAsset(creationRegistry, "scene", assetDirectory);
	Require(createMutation.Result.Succeeded(), "Expected workspace scene creation");
	Require(handledCreatePath == secondScenePath, "Expected controller to generate the next unique path");
	Require(createMutation.Guid == "created-guid" && createMutation.NewPath == secondScenePath, "Expected creation mutation identity");
	Require(createMutation.BeginRename, "Expected created assets to begin inline rename");

	workspaceController.SetRenameHandler([](const HE::AssetGuid& guid, std::string_view newName, HE::AssetRecord* outRecord) {
		Require(guid == "created-guid", "Expected rename handler guid");
		Require(newName == "Renamed", "Expected rename handler base name");
		outRecord->Guid = guid;
		outRecord->AbsolutePath = "Assets/Scenes/Renamed.scene";
		auto result = HE::ResultEnvelope::Success("asset.rename", guid, "Renamed");
		result.SetPayloadValue("old_asset_path", "Assets/Scenes/New Scene 1.scene");
		result.SetPayloadValue("asset_path", "Assets/Scenes/Renamed.scene");
		return result;
	});
	const auto renameMutation = workspaceController.RenameAsset("created-guid", "Renamed");
	Require(renameMutation.Result.Succeeded(), "Expected workspace asset rename");
	Require(renameMutation.Guid == "created-guid", "Expected renamed asset guid stability");
	Require(renameMutation.OldPath == "Assets/Scenes/New Scene 1.scene", "Expected rename old path");
	Require(renameMutation.NewPath == "Assets/Scenes/Renamed.scene", "Expected rename new path");

	bool deleteHandled = false;
	workspaceController.SetDeleteHandler([&](const HE::AssetGuid& guid) {
		Require(guid == "created-guid", "Expected delete handler guid");
		deleteHandled = true;
		return HE::ResultEnvelope::Success("asset.delete", guid, "Deleted");
	});
	const auto deleteMutation = workspaceController.DeleteAsset("created-guid");
	Require(deleteMutation.Result.Succeeded() && deleteHandled, "Expected workspace asset deletion");
	Require(deleteMutation.Guid == "created-guid", "Expected deleted asset identity");

	const std::filesystem::path filePath = "Assets/Meshes/Quad.mesh";
	const auto fileAction = HE::MakeProjectReimportAction(filePath, false);
	Require(fileAction.Type == HE::ProjectPanelActionType::ReimportPath, "Expected file reimport action");
	Require(fileAction.Path == filePath, "Expected file target path");

	const auto directoryAction = HE::MakeProjectReimportAction("Assets/Meshes", false);
	Require(directoryAction.Type == HE::ProjectPanelActionType::ReimportPath, "Expected directory reimport action");

	const auto allAction = HE::MakeProjectReimportAction({}, true);
	Require(allAction.Type == HE::ProjectPanelActionType::ReimportAll, "Expected reimport all action");
	Require(allAction.Path.empty(), "Expected reimport all to defer asset root resolution to the editor");
	Require(HE::IsProjectPanelVisibleFile("Assets/Meshes/Quad.obj"), "Expected source assets to remain visible");
	Require(!HE::IsProjectPanelVisibleFile("Assets/Meshes/Quad.obj.meta"), "Expected metadata sidecars to remain hidden");
	Require(!HE::IsProjectPanelVisibleFile("Assets/Meshes/Quad.obj.META"), "Expected metadata sidecar matching to ignore case");
	std::vector<std::string> assetNames{ "Asset10.scene", "beta.scene", "asset2.scene", "Alpha.scene", "asset01.scene", "asset1.scene" };
	std::sort(assetNames.begin(), assetNames.end(), HE::ProjectAssetNameLess);
	const std::vector<std::string> expectedAssetNames{ "Alpha.scene", "asset1.scene", "asset01.scene", "asset2.scene", "Asset10.scene", "beta.scene" };
	Require(assetNames == expectedAssetNames, "Expected case-insensitive natural asset ordering");
	Require(HE::ProjectAssetNameLess("asset01a.scene", "asset1z.scene"), "Expected suffix ordering before numeric width tie-breaks");

	const auto createAction = HE::MakeProjectCreateAssetAction("scene", assetDirectory);
	Require(createAction.Type == HE::ProjectPanelActionType::CreateAsset, "Expected create asset action");
	Require(createAction.TypeId == "scene" && createAction.Path == assetDirectory, "Expected create action parameters");
	const auto renameAction = HE::MakeProjectRenameAssetAction("created-guid", "Renamed");
	Require(renameAction.Type == HE::ProjectPanelActionType::RenameAsset, "Expected rename asset action");
	Require(renameAction.Guid == "created-guid" && renameAction.Name == "Renamed", "Expected rename action parameters");
	const auto deleteAction = HE::MakeProjectDeleteAssetAction("created-guid");
	Require(deleteAction.Type == HE::ProjectPanelActionType::DeleteAsset, "Expected delete asset action");
	Require(deleteAction.Guid == "created-guid", "Expected delete action identity");

	HE::ProjectPanel projectPanel;
	HE::AssetRecord renameRecord;
	renameRecord.Guid = "created-guid";
	renameRecord.Source = HE::AssetSource::File;
	renameRecord.AbsolutePath = assetDirectory / "New Scene 1.scene";
	const std::array renameRecords{ renameRecord };
	projectPanel.SetAssetRecords(renameRecords);
	projectPanel.BeginRename(renameRecord.Guid);
	Require(projectPanel.IsRenaming(), "Expected inline rename to start");
	projectPanel.RetryRename(renameRecord.Guid, "Attempted Name");
	Require(projectPanel.IsRenaming(), "Expected failed rename to keep inline editing active");
	Require(projectPanel.GetRenameDraft() == "Attempted Name", "Expected failed rename to preserve the submitted draft");
	projectPanel.QueueAssetSelection("other-guid", assetDirectory / "Other.scene");
	Require(!projectPanel.ConsumePendingAction().has_value(), "Expected selection to wait while rename is active");
	projectPanel.CompleteRename(renameRecord.Guid);
	Require(!projectPanel.IsRenaming(), "Expected successful rename confirmation to end inline editing");
	const auto deferredSelection = projectPanel.ConsumePendingAction();
	Require(deferredSelection && deferredSelection->Type == HE::ProjectPanelActionType::SelectAsset, "Expected deferred selection after rename completion");
	Require(deferredSelection->Guid == "other-guid", "Expected deferred selection identity");

	std::filesystem::remove_all(smokeRoot, errorCode);

	std::cout << "ProjectPanelActionSmoke passed" << std::endl;
	return 0;
}
