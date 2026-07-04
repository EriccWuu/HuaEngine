#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

#include "AssetTypes.h"

namespace HE {
	struct AssetRecord {
		AssetHandle Handle = 0;
		AssetGuid Guid;
		AssetKind Kind = AssetKind::Unknown;
		AssetSource Source = AssetSource::Unknown;
		std::string AssetId;
		std::filesystem::path RelativePath;
		std::filesystem::path AbsolutePath;
		std::string BuiltinName;
		AssetImportState ImportState = AssetImportState::Unknown;
		bool ExistsOnDisk = false;

		[[nodiscard]] bool IsOperational() const {
			return !Guid.empty() && !AssetId.empty() && Kind != AssetKind::Unknown && Source != AssetSource::Unknown;
		}
	};

	class AssetRegistry {
	public:
		[[nodiscard]] AssetHandle Upsert(AssetRecord record) {
			if (record.AssetId.empty()) {
				return 0;
			}

			AssetHandle handle = record.Handle;
			const auto existingAssetIdIt = m_AssetIds.find(record.AssetId);
			if (existingAssetIdIt != m_AssetIds.end()) {
				handle = existingAssetIdIt->second;
			}
			else if (!record.Guid.empty()) {
				const auto existingGuidIt = m_Guids.find(record.Guid);
				if (existingGuidIt != m_Guids.end()) {
					handle = existingGuidIt->second;
				}
			}

			if (handle == 0) {
				handle = m_NextHandle++;
			}

			auto existingRecordIt = m_Assets.find(handle);
			if (existingRecordIt != m_Assets.end()) {
				m_AssetIds.erase(existingRecordIt->second.AssetId);
				if (!existingRecordIt->second.Guid.empty()) {
					m_Guids.erase(existingRecordIt->second.Guid);
				}
			}

			record.Handle = handle;
			if (handle >= m_NextHandle) {
				m_NextHandle = handle + 1;
			}
			m_AssetIds[record.AssetId] = handle;
			if (!record.Guid.empty()) {
				m_Guids[record.Guid] = handle;
			}
			m_Assets[handle] = std::move(record);
			return handle;
		}

		[[nodiscard]] bool Contains(AssetHandle handle) const {
			return m_Assets.find(handle) != m_Assets.end();
		}

		[[nodiscard]] bool Contains(std::string_view assetId) const {
			return m_AssetIds.find(std::string(assetId)) != m_AssetIds.end();
		}

		[[nodiscard]] bool ContainsGuid(const AssetGuid& guid) const {
			return m_Guids.find(guid) != m_Guids.end();
		}

		[[nodiscard]] const AssetRecord* Find(AssetHandle handle) const {
			const auto it = m_Assets.find(handle);
			return it != m_Assets.end() ? &it->second : nullptr;
		}

		[[nodiscard]] AssetRecord* FindMutable(AssetHandle handle) {
			const auto it = m_Assets.find(handle);
			return it != m_Assets.end() ? &it->second : nullptr;
		}

		[[nodiscard]] const AssetRecord* Find(std::string_view assetId) const {
			const auto assetIdIt = m_AssetIds.find(std::string(assetId));
			if (assetIdIt == m_AssetIds.end()) {
				return nullptr;
			}

			return Find(assetIdIt->second);
		}

		[[nodiscard]] AssetRecord* FindMutable(std::string_view assetId) {
			const auto assetIdIt = m_AssetIds.find(std::string(assetId));
			if (assetIdIt == m_AssetIds.end()) {
				return nullptr;
			}

			return FindMutable(assetIdIt->second);
		}

		[[nodiscard]] const AssetRecord* FindByGuid(const AssetGuid& guid) const {
			const auto guidIt = m_Guids.find(guid);
			if (guidIt == m_Guids.end()) {
				return nullptr;
			}

			return Find(guidIt->second);
		}

		[[nodiscard]] AssetRecord* FindMutableByGuid(const AssetGuid& guid) {
			const auto guidIt = m_Guids.find(guid);
			if (guidIt == m_Guids.end()) {
				return nullptr;
			}

			return FindMutable(guidIt->second);
		}

		[[nodiscard]] size_t GetAssetCount() const {
			return m_Assets.size();
		}

		template<typename Callback>
		void ForEachRecord(Callback&& callback) const {
			for (const auto& [handle, record] : m_Assets) {
				(void)handle;
				callback(record);
			}
		}

	private:
		AssetHandle m_NextHandle = 1;
		std::unordered_map<AssetHandle, AssetRecord> m_Assets;
		std::unordered_map<std::string, AssetHandle> m_AssetIds;
		std::unordered_map<AssetGuid, AssetHandle> m_Guids;
	};
}
