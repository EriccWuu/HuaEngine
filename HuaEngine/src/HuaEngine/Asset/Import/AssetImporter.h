#pragma once

#include <filesystem>
#include <string_view>
#include <vector>

#include "HuaEngine/Asset/AssetManifest.h"
#include "HuaEngine/Asset/Library/AssetLibraryTypes.h"
#include "HuaEngine/Core/ResultEnvelope.h"
#include "HuaEngine/Project/ProjectContext.h"

namespace HE {
	struct AssetImportContext {
		const ProjectContext& Project;
		const AssetManifestRecord& SourceAsset;
		std::filesystem::path SourcePath;
		const AssetManifest* Manifest = nullptr;
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
		[[nodiscard]] virtual AssetImportResult Import(const AssetImportContext& context) const = 0;
	};
}
