#pragma once

#include "AssetImporter.h"
#include "ObjMeshImportSettings.h"
#include "HuaEngine/Asset/Artifact/MeshArtifact.h"

namespace HE {
	class ObjMeshImporter final : public AssetImporter {
	public:
		[[nodiscard]] std::string_view GetId() const override { return "hua.mesh-obj"; }
		[[nodiscard]] uint32_t GetVersion() const override { return 1; }
		[[nodiscard]] uint32_t GetArtifactVersion() const override { return MeshArtifactVersion; }
		[[nodiscard]] bool CanImport(AssetKind kind, std::string_view extension) const override;
		[[nodiscard]] std::unique_ptr<AssetImportSettings> CreateDefaultSettings() const override;
		[[nodiscard]] ResultEnvelope DecodeSettings(const AssetMetaSettingsNode& source, std::unique_ptr<AssetImportSettings>& output) const override;
		[[nodiscard]] ResultEnvelope EncodeSettings(const AssetImportSettings& settings, AssetMetaSettingsNode& output) const override;
		[[nodiscard]] ResultEnvelope ValidateSettings(const AssetImportSettings& settings) const override;
		[[nodiscard]] AssetImportResult Import(const AssetImportContext& context) const override;
	};
}
