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
		[[nodiscard]] ResultEnvelope ResolveTexture(const AssetGuid& guid, Ref<Rendering::TextureResource>& outTexture);
		[[nodiscard]] ResultEnvelope ResolveShader(const AssetGuid& guid, Ref<Rendering::ShaderProgram>& outShader);

	private:
		AssetService* m_Service = nullptr;
	};
}
