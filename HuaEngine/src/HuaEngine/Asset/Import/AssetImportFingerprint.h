#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "HuaEngine/Asset/AssetTypes.h"
#include "HuaEngine/Core/ResultEnvelope.h"

namespace HE {
	struct AssetImportSourceInput {
		std::string NormalizedPath;
		std::string ContentHash;
	};

	struct AssetImportDependencyInput {
		AssetGuid Guid;
		std::string ArtifactDigest;
	};

	struct AssetImportFingerprintInput {
		std::string ImporterId;
		uint32_t ImporterVersion = 0;
		uint32_t ArtifactVersion = 0;
		std::vector<AssetImportSourceInput> Sources;
		std::vector<AssetImportDependencyInput> Dependencies;
		std::vector<std::pair<std::string, std::string>> Options;
	};

	ResultEnvelope ComputeAssetImportFingerprint(const AssetImportFingerprintInput& input, std::string& outFingerprint);
}
