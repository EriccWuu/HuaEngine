#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "HuaEngine/Asset/AssetRegistry.h"
#include "Panels/AssetPickerModel.h"
#include "Panels/RuntimeInspector.h"

namespace {
	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[EditorAssetPickerSmoke] " << message << std::endl;
			std::exit(1);
		}
	}
}

int main() {
	const std::vector<HE::AssetRecord> records = {
		{ .Guid = "mesh-zebra", .Kind = HE::AssetKind::Mesh, .Source = HE::AssetSource::File, .AssetId = "Meshes/Zebra.mesh" },
		{ .Guid = "material-default", .Kind = HE::AssetKind::Material, .Source = HE::AssetSource::Builtin, .AssetId = "builtin/material/default" },
		{ .Guid = "material-metal", .Kind = HE::AssetKind::Material, .Source = HE::AssetSource::File, .AssetId = "Materials/Metal.mat" },
		{ .Guid = "material-plastic", .Kind = HE::AssetKind::Material, .Source = HE::AssetSource::File, .AssetId = "Materials/Plastic.mat" },
		{ .Guid = "mesh-alpha", .Kind = HE::AssetKind::Mesh, .Source = HE::AssetSource::File, .AssetId = "Meshes/alpha.mesh" }
	};

	const auto options = HE::Editor::BuildAssetPickerOptions(records, HE::AssetKind::Mesh);
	Require(options.size() == 2, "Expected the picker to include only mesh assets");
	Require(options[0].Guid == "mesh-alpha", "Expected mesh options to be sorted case-insensitively by display name");
	Require(options[0].DisplayName == "Meshes/alpha.mesh", "Expected the asset id to be used as the readable display name");
	Require(options[1].Guid == "mesh-zebra", "Expected all matching mesh assets to remain available");

	Require(
		HE::Editor::GetAssetPickerPreview(options, {}).DisplayName == "None",
		"Expected an empty asset reference to display as None");
	Require(
		HE::Editor::GetAssetPickerPreview(options, "mesh-zebra").DisplayName == "Meshes/Zebra.mesh",
		"Expected a known GUID to resolve to its readable asset name");
	const auto missingPreview = HE::Editor::GetAssetPickerPreview(options, "orphan-guid");
	Require(missingPreview.Missing, "Expected an unresolved GUID to be marked missing");
	Require(missingPreview.DisplayName == "Missing: orphan-guid", "Expected a missing reference to preserve its GUID in the preview");

	Require(
		HE::Editor::AssetPickerOptionMatches(options[1], "zEbRa"),
		"Expected picker search to be case-insensitive");
	Require(
		!HE::Editor::AssetPickerOptionMatches(options[0], "mesh-alpha"),
		"Expected resolved asset GUIDs to remain internal and excluded from search");
	Require(
		!HE::Editor::AssetPickerOptionMatches(options[0], "sphere"),
		"Expected unrelated assets to be filtered out");

	const auto materialOptions = HE::Editor::BuildAssetPickerOptions(records, HE::AssetKind::Material);
	Require(materialOptions.size() == 3, "Expected the picker to include material assets");
	Require(materialOptions[0].Guid == "material-default", "Expected builtin material in sorted options");
	Require(materialOptions[1].Guid == "material-metal", "Expected imported material in sorted options");
	Require(materialOptions[2].Guid == "material-plastic", "Expected all imported materials in sorted options");

	const HE::Editor::RuntimeInspectorContext context{
		.MeshAssets = options,
		.MaterialAssets = materialOptions
	};
	Require(context.GetAssetOptions(HE::AssetKind::Mesh).front().Guid == "mesh-alpha", "Expected mesh options selected by kind");
	Require(context.GetAssetOptions(HE::AssetKind::Material).front().Guid == "material-default", "Expected material options selected by kind");
	Require(context.GetAssetOptions(HE::AssetKind::Texture2D).empty(), "Expected unsupported picker kind to return no options");

	std::cout << "EditorAssetPickerSmoke passed" << std::endl;
	return 0;
}
