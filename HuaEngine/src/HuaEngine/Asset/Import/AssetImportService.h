#pragma once

#include <cstdint>

#include "AssetImporterRegistry.h"
#include "HuaEngine/Asset/AssetManifest.h"
#include "HuaEngine/Asset/Library/AssetLibrary.h"
#include "HuaEngine/Core/ResultEnvelope.h"
#include "HuaEngine/Project/ProjectContext.h"

namespace HE {
	struct AssetImportReport {
		uint32_t TotalFileAssets = 0;
		uint32_t ImportedAssets = 0;
		uint32_t SkippedAssets = 0;
		uint32_t FailedAssets = 0;
	};

	class AssetImportService {
	public:
		AssetImportService(
			const AssetImporterRegistry& registry,
			AssetLibrary& library)
			: m_Registry(&registry), m_Library(&library) {}

		ResultEnvelope ImportMissingAssets(
			const ProjectContext& context,
			const AssetManifest& manifest,
			AssetImportReport* outReport = nullptr) const;

	private:
		const AssetImporterRegistry* m_Registry = nullptr;
		AssetLibrary* m_Library = nullptr;
	};
}
