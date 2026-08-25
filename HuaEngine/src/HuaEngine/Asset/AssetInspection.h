#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "AssetRegistry.h"
#include "HuaEngine/Core/ResultEnvelope.h"

namespace HE {
	enum class AssetImportHealthState : uint8_t {
		Current = 0,
		LastGoodWithFailure,
		Missing,
		Stale
	};

	struct AssetImportHealth {
		AssetImportHealthState State = AssetImportHealthState::Missing;
		AssetGuid Guid;
		std::vector<DiagnosticEntry> Diagnostics;
	};

	struct AssetInspectionSnapshot {
		AssetRecord Asset;
		std::string ImporterId;
		uint32_t ImporterVersion = 0;
		uint32_t SettingsVersion = 0;
		AssetImportHealth Health;
		std::string ImportFingerprint;
		std::string SourceContentHash;
		std::string MetaContentHash;
		std::filesystem::path ArtifactRelativePath;
		std::vector<AssetGuid> Dependencies;
		std::vector<AssetGuid> Dependents;
		std::vector<DiagnosticEntry> Diagnostics;
	};
}
