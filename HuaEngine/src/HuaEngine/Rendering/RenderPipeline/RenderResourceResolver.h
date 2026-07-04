#pragma once

#include <vector>

#include "HuaEngine/Rendering/RenderPipeline/RenderTypes.h"

namespace HE {
	class AssetResolver;
}

namespace HE::Rendering {
	class RenderResourceResolver {
	public:
		RenderResourceResolver() = default;
		explicit RenderResourceResolver(HE::AssetResolver& assetResolver);

		void SetAssetResolver(HE::AssetResolver* assetResolver) { m_AssetResolver = assetResolver; }

		bool Resolve(
			const RenderItem& item,
			ResolvedRenderItem& outResolvedItem,
			RenderStats& stats,
			std::vector<RenderDiagnostic>& diagnostics) const;

	private:
		HE::AssetResolver* m_AssetResolver = nullptr;
	};
}
