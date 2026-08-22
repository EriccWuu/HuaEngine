#pragma once

#include "HuaEngine/Asset/Library/AssetLibraryTypes.h"
#include "HuaEngine/Core/Assert.h"
#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Core/ResultEnvelope.h"
#include "HuaEngine/Rendering/Mesh/MeshCore.h"

namespace HE {
	inline constexpr uint32_t MeshArtifactVersion = 1;

	ResultEnvelope EncodeMeshArtifact(
		const Rendering::Mesh& mesh,
		AssetArtifact& outArtifact);

	ResultEnvelope DecodeMeshArtifact(
		const AssetArtifact& artifact,
		Ref<Rendering::Mesh>& outMesh);
}
