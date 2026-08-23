#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

#include "AssetManifest.h"
#include "AssetRegistry.h"
#include "AssetRuntimeCache.h"
#include "HuaEngine/Asset/Import/AssetImporterRegistry.h"
#include "HuaEngine/Asset/Import/AssetImportService.h"
#include "HuaEngine/Asset/Library/AssetLibrary.h"
#include "HuaEngine/Core/ResultEnvelope.h"
#include "HuaEngine/Project/ProjectContext.h"
#include "HuaEngine/Rendering/Material/Material.h"
#include "HuaEngine/Rendering/Mesh/MeshCore.h"
#include "HuaEngine/Rendering/RHI/TextureResource.h"

namespace HE {
	struct AssetReimportReport {
		uint32_t ScannedFiles = 0;
		uint32_t SupportedFiles = 0;
		uint32_t RegisteredAssets = 0;
		uint32_t ReimportedAssets = 0;
		uint32_t SkippedFiles = 0;
		uint32_t FailedAssets = 0;
	};

	struct AssetValidationReport {
		uint32_t TotalAssets = 0;
		uint32_t MeshAssets = 0;
		uint32_t MaterialAssets = 0;
		uint32_t TextureAssets = 0;
		uint32_t UnknownKindAssets = 0;
		uint32_t InvalidAssetRecords = 0;
		uint32_t AssetsOutsideProjectRoot = 0;
		uint32_t MissingFileAssets = 0;
		uint32_t MissingBuiltinAssets = 0;
		uint32_t BuiltinMetadataIssues = 0;
		uint32_t FileAssetsMissingArtifacts = 0;
		uint32_t FileAssetsWithoutImporter = 0;
		uint32_t BuiltinAssetsMissingArtifacts = 0;
		uint32_t BuiltinAssetsWithoutImporter = 0;
		uint32_t FallbackAssets = 0;

		[[nodiscard]] bool IsOperational() const {
			return UnknownKindAssets == 0 &&
				InvalidAssetRecords == 0 &&
				AssetsOutsideProjectRoot == 0 &&
				MissingFileAssets == 0 &&
				MissingBuiltinAssets == 0 &&
				BuiltinMetadataIssues == 0 &&
				FileAssetsMissingArtifacts == 0 &&
				FileAssetsWithoutImporter == 0 &&
				BuiltinAssetsMissingArtifacts == 0 &&
				BuiltinAssetsWithoutImporter == 0;
		}

		[[nodiscard]] uint32_t MetadataIssueCount() const {
			return UnknownKindAssets + InvalidAssetRecords + AssetsOutsideProjectRoot + MissingFileAssets + MissingBuiltinAssets + BuiltinMetadataIssues;
		}

		[[nodiscard]] uint32_t RuntimeIssueCount() const {
			return FileAssetsMissingArtifacts + FileAssetsWithoutImporter + BuiltinAssetsMissingArtifacts + BuiltinAssetsWithoutImporter;
		}

		[[nodiscard]] bool HasIssues() const {
			return !IsOperational();
		}
	};

	class ENGINE_API AssetService {
	public:
		AssetService();

		[[nodiscard]] ResultEnvelope LoadOrCreateManifest(const ProjectContext& context);
		[[nodiscard]] ResultEnvelope LoadManifestReadOnly(const ProjectContext& context);
		[[nodiscard]] ResultEnvelope InitializeProjectAssets(
			const ProjectContext& context,
			AssetImportReport* outReport = nullptr);
		[[nodiscard]] bool CanImportSource(const std::filesystem::path& sourcePath) const;
		[[nodiscard]] ResultEnvelope ReimportAssets(
			const ProjectContext& context,
			const std::filesystem::path& targetPath,
			AssetReimportReport* outReport = nullptr);

		[[nodiscard]] ResultEnvelope CreateBuiltinMeshAsset(
			const ProjectContext& context,
			std::string_view assetId,
			BuiltinMeshPrimitive primitive,
			std::string_view meshName = {},
			AssetHandle* outHandle = nullptr);

		[[nodiscard]] ResultEnvelope RegisterMeshAsset(
			const ProjectContext& context,
			std::string_view assetId,
			const Ref<Rendering::Mesh>& mesh,
			AssetHandle* outHandle = nullptr);

		[[nodiscard]] ResultEnvelope LoadMeshAsset(
			const ProjectContext& context,
			std::string_view assetId,
			AssetHandle* outHandle = nullptr);

		[[nodiscard]] ResultEnvelope RegisterMaterialAsset(
			const ProjectContext& context,
			std::string_view assetId,
			const Ref<Rendering::Material>& material,
			AssetHandle* outHandle = nullptr);

		[[nodiscard]] ResultEnvelope LoadMaterialAsset(
			const ProjectContext& context,
			std::string_view assetId,
			AssetHandle* outHandle = nullptr);

		[[nodiscard]] ResultEnvelope RegisterTextureAsset(
			const ProjectContext& context,
			std::string_view assetId,
			const Ref<Rendering::TextureResource>& texture = nullptr,
			AssetHandle* outHandle = nullptr);

		[[nodiscard]] ResultEnvelope ResolveAsset(AssetHandle handle, AssetRecord& outRecord) const;
		[[nodiscard]] ResultEnvelope ResolveAsset(std::string_view assetId, AssetRecord& outRecord) const;
		[[nodiscard]] ResultEnvelope ResolveMeshAsset(AssetHandle handle, Ref<Rendering::Mesh>& outMesh) const;
		[[nodiscard]] ResultEnvelope ResolveMaterialAsset(AssetHandle handle, Ref<Rendering::Material>& outMaterial) const;
		[[nodiscard]] ResultEnvelope ResolveTextureAsset(AssetHandle handle, Ref<Rendering::TextureResource>& outTexture) const;
		[[nodiscard]] ResultEnvelope ValidateRegistry(const ProjectContext& context, AssetValidationReport* outReport = nullptr);

		[[nodiscard]] const AssetRegistry& GetAssetRegistry() const { return m_Registry; }
		[[nodiscard]] AssetRegistry& GetAssetRegistry() { return m_Registry; }
		[[nodiscard]] const AssetManifest& GetManifest() const { return m_Manifest; }
		[[nodiscard]] AssetManifest& GetManifest() { return m_Manifest; }
		[[nodiscard]] AssetRuntimeCache& GetRuntimeCache() { return m_RuntimeCache; }
		[[nodiscard]] const AssetRuntimeCache& GetRuntimeCache() const { return m_RuntimeCache; }
		[[nodiscard]] AssetLibrary& GetLibrary() { return m_Library; }
		[[nodiscard]] const AssetLibrary& GetLibrary() const { return m_Library; }
		[[nodiscard]] AssetImporterRegistry& GetImporterRegistry() { return m_ImporterRegistry; }
		[[nodiscard]] const AssetImporterRegistry& GetImporterRegistry() const { return m_ImporterRegistry; }
		[[nodiscard]] bool IsManifestLoaded() const { return !m_Manifest.Empty(); }
		[[nodiscard]] const ProjectContext* GetProjectContext() const { return m_ProjectContext ? &*m_ProjectContext : nullptr; }
		[[nodiscard]] const AssetRecord* FindRecordByGuid(const AssetGuid& guid) const;

	private:
		[[nodiscard]] ResultEnvelope LoadOrCreateManifestInternal(
			const ProjectContext& context,
			bool resetRuntimeCache);

		AssetRegistry m_Registry;
		AssetManifest m_Manifest;
		AssetRuntimeCache m_RuntimeCache;
		AssetLibrary m_Library;
		AssetImporterRegistry m_ImporterRegistry;
		std::optional<ProjectContext> m_ProjectContext;
	};
}
