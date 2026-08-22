#pragma once

#include <filesystem>

#include "AssetLibraryTypes.h"
#include "HuaEngine/Core/ResultEnvelope.h"

namespace HE {
	ResultEnvelope WriteAssetArtifactFile(
		const std::filesystem::path& path,
		const AssetArtifact& artifact);

	ResultEnvelope ReadAssetArtifactFile(
		const std::filesystem::path& path,
		AssetArtifact& outArtifact,
		AssetArtifactHeader* outHeader = nullptr);

	ResultEnvelope WriteAssetBinaryFileAtomically(
		const std::filesystem::path& path,
		const std::vector<uint8_t>& bytes,
		std::string operation);

	ResultEnvelope ReadAssetBinaryFile(
		const std::filesystem::path& path,
		std::vector<uint8_t>& outBytes,
		std::string operation);
}
