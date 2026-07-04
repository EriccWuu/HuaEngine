#pragma once

#include "AssetManifest.h"
#include "AssetRuntimeCache.h"
#include "HuaEngine/Core/ResultEnvelope.h"

namespace HE {
	class AssetService;

	class AssetResolver {
	public:
		explicit AssetResolver(AssetService& service);

		[[nodiscard]] ResultEnvelope ResolveMesh(const AssetGuid& guid, Ref<Rendering::Mesh>& outMesh);
		[[nodiscard]] ResultEnvelope ResolveMaterial(const AssetGuid& guid, Ref<Rendering::Material>& outMaterial);
		[[nodiscard]] ResultEnvelope ResolveTexture(const AssetGuid& guid, Ref<Rendering::Texture2D>& outTexture);

	private:
		AssetService* m_Service = nullptr;

		[[nodiscard]] Ref<Rendering::Mesh> CreateBuiltinMesh(const AssetManifestRecord& record) const;
		[[nodiscard]] Ref<Rendering::Material> CreateBuiltinMaterial(const AssetManifestRecord& record) const;
	};
}
