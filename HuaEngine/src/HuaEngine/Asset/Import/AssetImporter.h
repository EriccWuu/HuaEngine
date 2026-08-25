#pragma once

#include <filesystem>
#include <memory>
#include <string_view>
#include <vector>

#include "HuaEngine/Asset/AssetManifest.h"
#include "HuaEngine/Asset/Import/AssetImportFingerprint.h"
#include "HuaEngine/Asset/Library/AssetLibraryTypes.h"
#include "HuaEngine/Asset/Metadata/AssetImportSettings.h"
#include "HuaEngine/Asset/Metadata/AssetMeta.h"
#include "HuaEngine/Core/ResultEnvelope.h"
#include "HuaEngine/Project/ProjectContext.h"

namespace HE {
	class AssetLibrary;
	struct AssetImportContext {
		const ProjectContext& Project;
		const AssetManifestRecord& SourceAsset;
		std::filesystem::path SourcePath;
		const AssetManifest* Manifest = nullptr;
		const AssetLibrary* Library = nullptr;
		const AssetImportSettings* Settings = nullptr;
	};

	struct AssetImportResult {
		bool Success = false;
		AssetArtifact Artifact;
		std::vector<DiagnosticEntry> Diagnostics;
	};

	class AssetImporter {
	public:
		virtual ~AssetImporter() = default;

		[[nodiscard]] virtual std::string_view GetId() const = 0;
		[[nodiscard]] virtual uint32_t GetVersion() const = 0;
		[[nodiscard]] virtual uint32_t GetArtifactVersion() const = 0;
		[[nodiscard]] virtual bool CanImport(AssetKind kind, std::string_view extension) const = 0;
		[[nodiscard]] virtual uint32_t GetSettingsVersion() const { return 1; }
		[[nodiscard]] virtual std::unique_ptr<AssetImportSettings> CreateDefaultSettings() const {
			return std::make_unique<EmptyAssetImportSettings>(std::string(GetId()));
		}
		[[nodiscard]] virtual ResultEnvelope DecodeSettings(const AssetMetaSettingsNode& source, std::unique_ptr<AssetImportSettings>& output) const {
			output.reset();
			if (!source.Values.empty()) return ResultEnvelope::Failure("asset.import.settings.decode", std::string(GetId()), "This importer does not accept import settings");
			output = CreateDefaultSettings();
			return ResultEnvelope::Success("asset.import.settings.decode", std::string(GetId()), "Import settings decoded");
		}
		[[nodiscard]] virtual ResultEnvelope EncodeSettings(const AssetImportSettings& settings, AssetMetaSettingsNode& output) const {
			output = {};
			if (settings.GetImporterId() != GetId()) return ResultEnvelope::Failure("asset.import.settings.encode", std::string(GetId()), "Import settings type does not match the importer");
			return ResultEnvelope::Success("asset.import.settings.encode", std::string(GetId()), "Import settings encoded");
		}
		[[nodiscard]] virtual ResultEnvelope ValidateSettings(const AssetImportSettings& settings) const {
			return settings.GetImporterId() == GetId()
				? ResultEnvelope::Success("asset.import.settings.validate", std::string(GetId()), "Import settings are valid")
				: ResultEnvelope::Failure("asset.import.settings.validate", std::string(GetId()), "Import settings type does not match the importer");
		}
		[[nodiscard]] virtual ResultEnvelope CollectDependencies(
			const AssetImportContext& context,
			std::vector<AssetGuid>& output) const {
			output.clear();
			return ResultEnvelope::Success("asset.import.dependencies", context.SourceAsset.Guid, "Asset dependencies collected");
		}
		[[nodiscard]] virtual ResultEnvelope BuildFingerprintInput(
			const AssetImportContext& context,
			std::string_view rootSourceHash,
			AssetImportFingerprintInput& output) const {
			output = {
				.ImporterId = std::string(GetId()),
				.ImporterVersion = GetVersion(),
				.ArtifactVersion = GetArtifactVersion(),
				.Sources = { { "source", std::string(rootSourceHash) } }
			};
			return ResultEnvelope::Success("asset.import_fingerprint.inputs", context.SourceAsset.Guid, "Import fingerprint inputs collected");
		}
		[[nodiscard]] virtual AssetImportResult Import(const AssetImportContext& context) const = 0;
	};
}
