#pragma once

#include "AssetImporter.h"
#include "HuaEngine/Asset/Artifact/MaterialArtifact.h"

namespace HE {
	class MaterialAssetImporter final : public AssetImporter {
	public:
		[[nodiscard]] std::string_view GetId() const override { return "hua.material-yaml"; }
		[[nodiscard]] uint32_t GetVersion() const override { return 5; }
		[[nodiscard]] uint32_t GetArtifactVersion() const override { return MaterialArtifactVersion; }
		[[nodiscard]] bool CanImport(AssetKind kind, std::string_view extension) const override;
		[[nodiscard]] ResultEnvelope CollectDependencies(const AssetImportContext& context, std::vector<AssetGuid>& output) const override;
		[[nodiscard]] ResultEnvelope BuildFingerprintInput(const AssetImportContext& context, std::string_view rootSourceHash, AssetImportFingerprintInput& output) const override;
		[[nodiscard]] AssetImportResult Import(const AssetImportContext& context) const override;
	};
}
