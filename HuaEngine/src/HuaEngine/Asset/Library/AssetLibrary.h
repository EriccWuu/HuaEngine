#pragma once

#include <filesystem>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "AssetLibraryTypes.h"
#include "HuaEngine/Core/ResultEnvelope.h"
#include "HuaEngine/Project/ProjectContext.h"

namespace HE {
	class AssetLibrary {
	public:
		ResultEnvelope Open(const ProjectContext& context);
		ResultEnvelope Save() const;

		[[nodiscard]] const AssetLibraryRecord* Find(const AssetGuid& guid) const;
		[[nodiscard]] std::vector<AssetGuid> FindDependents(const AssetGuid& dependencyGuid) const;
		[[nodiscard]] bool IsArtifactAvailable(
			const AssetGuid& guid,
			AssetKind kind,
			std::string_view importerId,
			uint32_t importerVersion,
			uint32_t artifactVersion) const;
		[[nodiscard]] bool IsArtifactCurrent(
			const AssetGuid& guid,
			AssetKind kind,
			std::string_view importerId,
			uint32_t importerVersion,
			uint32_t artifactVersion,
			std::string_view importFingerprint) const;

		ResultEnvelope CommitArtifact(
			const AssetGuid& guid,
			std::string_view importerId,
			uint32_t importerVersion,
			std::string_view importFingerprint,
			const AssetArtifact& artifact);
		ResultEnvelope RecordImportFailure(
			const AssetGuid& guid,
			std::string_view importFingerprint,
			const std::vector<DiagnosticEntry>& diagnostics);

		ResultEnvelope ReadArtifact(
			const AssetGuid& guid,
			AssetArtifact& outArtifact) const;

		[[nodiscard]] const std::filesystem::path& GetRootPath() const { return m_RootPath; }
		[[nodiscard]] const std::filesystem::path& GetCatalogPath() const { return m_CatalogPath; }

	private:
		[[nodiscard]] ResultEnvelope SaveRecords(const std::unordered_map<AssetGuid, AssetLibraryRecord>& records) const;
		[[nodiscard]] bool LoadCatalog(std::string& outError);
		[[nodiscard]] bool IsSafeArtifactPath(const std::filesystem::path& relativePath) const;
		[[nodiscard]] std::filesystem::path ResolveArtifactPath(const std::filesystem::path& relativePath) const;

		std::filesystem::path m_RootPath;
		std::filesystem::path m_ArtifactRootPath;
		std::filesystem::path m_CatalogPath;
		std::unordered_map<AssetGuid, AssetLibraryRecord> m_Records;
		bool m_IsOpen = false;
	};
}
