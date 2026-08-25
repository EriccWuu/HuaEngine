#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "AssetRegistry.h"
#include "HuaEngine/Asset/Artifact/ShaderArtifact.h"
#include "HuaEngine/Core/ResultEnvelope.h"

namespace HE {
	struct MeshArtifactStatistics {
		uint32_t VertexCount = 0;
		uint32_t IndexCount = 0;
		std::array<float, 3> BoundsMin{};
		std::array<float, 3> BoundsMax{};
		bool HasUv = false;
		bool HasNormals = false;
		bool HasTangents = false;
	};

	struct TextureArtifactStatistics {
		uint32_t SourceWidth = 0;
		uint32_t SourceHeight = 0;
		uint32_t SourceChannels = 0;
		uint32_t Width = 0;
		uint32_t Height = 0;
		uint32_t MipLevels = 0;
		bool HasAlpha = false;
	};

	struct SceneAssetStatistics {
		std::string Name;
		uint32_t FormatVersion = 0;
		uint32_t EntityCount = 0;
	};
	enum class AssetImportHealthState : uint8_t {
		Current = 0,
		LastGoodWithFailure,
		Missing,
		Stale,
		NotApplicable
	};

	struct AssetImportHealth {
		AssetImportHealthState State = AssetImportHealthState::Missing;
		AssetGuid Guid;
		std::vector<DiagnosticEntry> Diagnostics;
	};

	struct AssetInspectionSnapshot {
		AssetRecord Asset;
		std::string ImporterId;
		uint32_t ImporterVersion = 0;
		uint32_t SettingsVersion = 0;
		AssetImportHealth Health;
		std::string ImportFingerprint;
		std::string SourceContentHash;
		std::string MetaContentHash;
		std::filesystem::path ArtifactRelativePath;
		std::vector<AssetGuid> Dependencies;
		std::vector<AssetGuid> Dependents;
		std::vector<DiagnosticEntry> Diagnostics;
		std::optional<MeshArtifactStatistics> MeshStatistics;
		std::optional<TextureArtifactStatistics> TextureStatistics;
		std::optional<ShaderArtifactDataV2> ShaderData;
		std::optional<SceneAssetStatistics> SceneStatistics;
	};
}
