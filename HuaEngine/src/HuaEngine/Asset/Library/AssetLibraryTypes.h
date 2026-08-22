#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "HuaEngine/Asset/AssetTypes.h"

namespace HE {
	inline constexpr uint32_t AssetArtifactContainerVersion = 1;
	inline constexpr uint32_t AssetLibraryFormatVersion = 1;

	struct AssetArtifactHeader {
		uint32_t ContainerVersion = AssetArtifactContainerVersion;
		AssetKind Kind = AssetKind::Unknown;
		uint32_t ArtifactVersion = 0;
		uint64_t PayloadSize = 0;
	};

	struct AssetArtifact {
		AssetKind Kind = AssetKind::Unknown;
		uint32_t ArtifactVersion = 0;
		std::vector<uint8_t> Payload;
		std::vector<AssetGuid> Dependencies;
	};

	struct AssetLibraryRecord {
		AssetGuid Guid;
		AssetKind Kind = AssetKind::Unknown;
		std::string ImporterId;
		uint32_t ImporterVersion = 0;
		uint32_t ArtifactVersion = 0;
		std::filesystem::path ArtifactRelativePath;
		std::vector<AssetGuid> Dependencies;
	};
}
