#pragma once

#include "AssetEditCommit.h"
#include "HuaEngine/Asset/AssetService.h"

namespace HE {
	class AssetAuthoringService {
	public:
		explicit AssetAuthoringService(AssetService& assets) : m_Assets(&assets) {}

		[[nodiscard]] ResultEnvelope Apply(
			const ProjectContext& context,
			const AssetEditCommit& commit,
			AssetApplyState& outState,
			AssetReimportReport* outReport = nullptr);

	private:
		AssetService* m_Assets = nullptr;
	};
}
