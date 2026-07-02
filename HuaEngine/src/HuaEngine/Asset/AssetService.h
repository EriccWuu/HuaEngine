#pragma once

#include <string_view>

#include "AssetRegistry.h"
#include "HuaEngine/Core/ResultEnvelope.h"
#include "HuaEngine/Project/ProjectContext.h"

namespace HE {
	struct AssetValidationReport {
		uint32_t TotalAssets = 0;
		uint32_t MeshAssets = 0;
		uint32_t MaterialAssets = 0;
		uint32_t TextureAssets = 0;
		uint32_t UnknownKindAssets = 0;
		uint32_t InvalidAssetRecords = 0;
		uint32_t AssetsOutsideProjectRoot = 0;
		uint32_t MeshAssetsMissingRuntimePayload = 0;
		uint32_t MaterialAssetsMissingRuntimePayload = 0;
		uint32_t SourceOnlyTextureAssets = 0;

		[[nodiscard]] bool IsOperational() const {
			return UnknownKindAssets == 0 &&
				InvalidAssetRecords == 0 &&
				AssetsOutsideProjectRoot == 0 &&
				MeshAssetsMissingRuntimePayload == 0 &&
				MaterialAssetsMissingRuntimePayload == 0;
		}

		[[nodiscard]] bool HasIssues() const {
			return !IsOperational();
		}
	};

	class ENGINE_API AssetService {
	public:
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
			const Ref<Rendering::Texture2D>& texture = nullptr,
			AssetHandle* outHandle = nullptr);

		[[nodiscard]] ResultEnvelope ResolveAsset(AssetHandle handle, AssetRecord& outRecord) const;
		[[nodiscard]] ResultEnvelope ResolveAsset(std::string_view assetId, AssetRecord& outRecord) const;
		[[nodiscard]] ResultEnvelope ResolveMeshAsset(AssetHandle handle, Ref<Rendering::Mesh>& outMesh) const;
		[[nodiscard]] ResultEnvelope ResolveMaterialAsset(AssetHandle handle, Ref<Rendering::Material>& outMaterial) const;
		[[nodiscard]] ResultEnvelope ResolveTextureAsset(AssetHandle handle, Ref<Rendering::Texture2D>& outTexture) const;
		[[nodiscard]] ResultEnvelope ValidateRegistry(const ProjectContext& context, AssetValidationReport* outReport = nullptr) const;

		[[nodiscard]] const AssetRegistry& GetAssetRegistry() const { return m_Registry; }
		[[nodiscard]] AssetRegistry& GetAssetRegistry() { return m_Registry; }

	private:
		AssetRegistry m_Registry;
	};
}
