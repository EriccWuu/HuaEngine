#pragma once

#include <filesystem>
#include <string_view>
#include <vector>

#include "HuaEngine/Asset/AssetManifest.h"
#include "HuaEngine/Asset/Import/AssetImportFingerprint.h"
#include "HuaEngine/Asset/Library/AssetLibraryTypes.h"
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
