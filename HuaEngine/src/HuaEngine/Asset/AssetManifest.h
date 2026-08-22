#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "AssetTypes.h"
#include "HuaEngine/Core/ResultEnvelope.h"
#include "HuaEngine/Project/ProjectContext.h"

namespace HE {
	struct AssetManifestRecord {
		AssetGuid Guid;
		std::string AssetId;
		AssetKind Kind = AssetKind::Unknown;
		AssetSource Source = AssetSource::Unknown;
		std::filesystem::path RelativePath;
		std::string BuiltinName;
		AssetImportState ImportState = AssetImportState::Unknown;
	};

	class AssetManifest {
	public:
		[[nodiscard]] bool Empty() const { return m_Records.empty(); }
		[[nodiscard]] size_t Size() const { return m_Records.size(); }
		[[nodiscard]] const AssetManifestRecord* FindByGuid(const AssetGuid& guid) const;
		[[nodiscard]] AssetManifestRecord* FindMutableByGuid(const AssetGuid& guid);
		[[nodiscard]] const AssetManifestRecord* FindByAssetId(std::string_view assetId) const;
		[[nodiscard]] bool Upsert(AssetManifestRecord record);

		template<typename Callback>
		void ForEachRecord(Callback&& callback) const {
			for (const auto& record : m_Records) {
				callback(record);
			}
		}

	private:
		std::vector<AssetManifestRecord> m_Records;
		std::unordered_map<AssetGuid, size_t> m_GuidIndex;
		std::unordered_map<std::string, size_t> m_AssetIdIndex;
	};

	ResultEnvelope LoadOrCreateAssetManifest(const ProjectContext& context, AssetManifest& outManifest);
	ResultEnvelope LoadAssetManifest(const ProjectContext& context, AssetManifest& outManifest);
	ResultEnvelope LoadAssetManifest(const std::filesystem::path& manifestPath, AssetManifest& outManifest);
	ResultEnvelope SaveAssetManifest(const ProjectContext& context, const AssetManifest& manifest);
	std::filesystem::path GetAssetManifestPath(const ProjectContext& context);
}
