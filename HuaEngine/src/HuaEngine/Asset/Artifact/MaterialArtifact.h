#pragma once

#include "HuaEngine/Asset/Library/AssetLibraryTypes.h"
#include "HuaEngine/Core/ResultEnvelope.h"
#include "HuaEngine/Rendering/Material/MaterialSourceData.h"

namespace HE {
	inline constexpr uint32_t MaterialArtifactVersion = 1;

	ResultEnvelope EncodeMaterialArtifact(
		const Rendering::MaterialSourceData& material,
		AssetArtifact& outArtifact);

	ResultEnvelope DecodeMaterialArtifact(
		const AssetArtifact& artifact,
		Rendering::MaterialSourceData& outMaterial);
}
