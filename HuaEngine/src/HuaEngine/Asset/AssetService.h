#pragma once

#include <string_view>

#include "AssetManifest.h"
#include "AssetRegistry.h"
#include "AssetRuntimeCache.h"
#include "HuaEngine/Core/ResultEnvelope.h"
#include "HuaEngine/Project/ProjectContext.h"
#include "HuaEngine/Rendering/Material/Material.h"
#include "HuaEngine/Rendering/Mesh/MeshCore.h"
#include "HuaEngine/Rendering/RHI/TextureResource.h"

namespace HE {
	struct AssetValidationReport {
		uint32_t TotalAssets = 0;
		uint32_t MeshAssets = 0;
		uint32_t MaterialAssets = 0;
		uint32_t TextureAssets = 0;
		uint32_t UnknownKindAssets = 0;
		uint32_t InvalidAssetRecords = 0;
		uint32_t AssetsOutsideProjectRoot = 0;
		uint32_t MissingFileAssets = 0;
		uint32_t BuiltinMetadataIssues = 0;
		uint32_t MeshAssetsMissingRuntimePayload = 0;
		uint32_t MaterialAssetsMissingRuntimePayload = 0;
		uint32_t SourceOnlyTextureAssets = 0;
		uint32_t FallbackAssets = 0;

		[[nodiscard]] bool IsOperational() const {
			return UnknownKindAssets == 0 &&
				InvalidAssetRecords == 0 &&
				AssetsOutsideProjectRoot == 0 &&
				MissingFileAssets == 0 &&
				BuiltinMetadataIssues == 0 &&
				MeshAssetsMissingRuntimePayload == 0 &&
				MaterialAssetsMissingRuntimePayload == 0;
		}

		[[nodiscard]] uint32_t MetadataIssueCount() const {
			return UnknownKindAssets + InvalidAssetRecords + AssetsOutsideProjectRoot + MissingFileAssets + BuiltinMetadataIssues;
		}

		[[nodiscard]] uint32_t RuntimeIssueCount() const {
			return MeshAssetsMissingRuntimePayload + MaterialAssetsMissingRuntimePayload;
		}

		[[nodiscard]] bool HasIssues() const {
			return !IsOperational();
		}
	};

	class ENGINE_API AssetService {
	public:
		[[nodiscard]] ResultEnvelope LoadOrCreateManifest(const ProjectContext& context);
		[[nodiscard]] ResultEnvelope LoadManifestReadOnly(const ProjectContext& context);

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
		[[nodiscard]] ResultEnvelope ValidateRegistry(const ProjectContext& context, AssetValidationReport* outReport = nullptr) const;

		[[nodiscard]] const AssetRegistry& GetAssetRegistry() const { return m_Registry; }
		[[nodiscard]] AssetRegistry& GetAssetRegistry() { return m_Registry; }
		[[nodiscard]] const AssetManifest& GetManifest() const { return m_Manifest; }
		[[nodiscard]] AssetManifest& GetManifest() { return m_Manifest; }
		[[nodiscard]] AssetRuntimeCache& GetRuntimeCache() { return m_RuntimeCache; }
		[[nodiscard]] const AssetRuntimeCache& GetRuntimeCache() const { return m_RuntimeCache; }
		[[nodiscard]] bool IsManifestLoaded() const { return !m_Manifest.Empty(); }
		[[nodiscard]] const AssetRecord* FindRecordByGuid(const AssetGuid& guid) const;

	private:
		AssetRegistry m_Registry;
		AssetManifest m_Manifest;
		AssetRuntimeCache m_RuntimeCache;
	};
}
