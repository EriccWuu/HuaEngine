#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Rendering/Material/Material.h"
#include "HuaEngine/Rendering/Mesh/MeshCore.h"
#include "HuaEngine/Rendering/Texture.h"

namespace HE {
	enum class AssetKind {
		Unknown,
		Mesh,
		Material,
		Texture2D
	};

	enum class BuiltinMeshPrimitive {
		Quad,
		Cube,
		Sphere
	};

	using AssetHandle = uint64_t;

	struct AssetRecord {
		AssetHandle Handle = 0;
		AssetKind Kind = AssetKind::Unknown;
		std::string AssetId;
		std::filesystem::path RelativePath;
		std::filesystem::path AbsolutePath;
		bool ExistsOnDisk = false;
		Ref<Rendering::Mesh> Mesh;
		Ref<Rendering::Material> Material;
		Ref<Rendering::Texture2D> Texture;

		[[nodiscard]] bool HasRuntimeValue() const {
			return static_cast<bool>(Mesh) || static_cast<bool>(Material) || static_cast<bool>(Texture);
		}

		[[nodiscard]] bool IsOperational() const {
			return !AssetId.empty() && (ExistsOnDisk || HasRuntimeValue());
		}
	};

	inline constexpr std::string_view ToString(AssetKind kind) {
		switch (kind) {
		case AssetKind::Mesh:
			return "mesh";
		case AssetKind::Material:
			return "material";
		case AssetKind::Texture2D:
			return "texture2d";
		case AssetKind::Unknown:
		default:
			return "unknown";
		}
	}

	inline constexpr std::string_view ToString(BuiltinMeshPrimitive primitive) {
		switch (primitive) {
		case BuiltinMeshPrimitive::Quad:
			return "quad";
		case BuiltinMeshPrimitive::Cube:
			return "cube";
		case BuiltinMeshPrimitive::Sphere:
			return "sphere";
		}

		return "unknown";
	}

	class AssetRegistry {
	public:
		[[nodiscard]] AssetHandle Upsert(AssetRecord record) {
			if (record.AssetId.empty()) {
				return 0;
			}

			AssetHandle handle = record.Handle;
			const auto existingHandleIt = m_AssetIds.find(record.AssetId);
			if (existingHandleIt != m_AssetIds.end()) {
				handle = existingHandleIt->second;
			}
			else if (handle == 0) {
				handle = m_NextHandle++;
			}

			auto existingRecordIt = m_Assets.find(handle);
			if (existingRecordIt != m_Assets.end() && existingRecordIt->second.AssetId != record.AssetId) {
				m_AssetIds.erase(existingRecordIt->second.AssetId);
			}

			record.Handle = handle;
			m_AssetIds[record.AssetId] = handle;
			m_Assets[handle] = std::move(record);
			return handle;
		}

		[[nodiscard]] bool Contains(AssetHandle handle) const {
			return m_Assets.find(handle) != m_Assets.end();
		}

		[[nodiscard]] bool Contains(std::string_view assetId) const {
			return m_AssetIds.find(std::string(assetId)) != m_AssetIds.end();
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
	};
}
