#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "AssetImporterRegistry.h"
#include "HuaEngine/Asset/AssetManifest.h"
#include "HuaEngine/Asset/Library/AssetLibrary.h"
#include "HuaEngine/Core/ResultEnvelope.h"
#include "HuaEngine/Project/ProjectContext.h"

namespace HE {
	enum class AssetImportPolicy {
		MissingOnly,
		Force
	};

	struct AssetImportReport {
		uint32_t TotalFileAssets = 0;
		uint32_t TotalBuiltinAssets = 0;
		uint32_t ImportedAssets = 0;
		uint32_t SkippedAssets = 0;
		uint32_t FailedAssets = 0;
		std::vector<AssetGuid> ImportedAssetGuids;
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

		ResultEnvelope ImportAssets(
			const ProjectContext& context,
			const AssetManifest& manifest,
			std::span<const AssetGuid> assetGuids,
			AssetImportPolicy policy,
			AssetImportReport* outReport = nullptr) const;

	private:
		const AssetImporterRegistry* m_Registry = nullptr;
		AssetLibrary* m_Library = nullptr;
	};
}
