#pragma once

#include <array>

#include "HuaEngine/Asset/AssetTypes.h"

namespace HE {
	struct AssetReferenceCapability {
		Refl::TypeGuid ValueTypeGuid;
		AssetKind Kind;
		const AssetGuid* (*ReadGuid)(const void*) noexcept;
		bool (*WriteGuid)(void*, const AssetGuid&);
	};

	namespace AssetReferenceDetail {
		inline const AssetGuid* ReadBaseGuid(const void* value) noexcept {
			return value ? &static_cast<const AssetReference*>(value)->Guid : nullptr;
		}

		inline bool WriteBaseGuid(void* value, const AssetGuid& guid) {
			if (!value) return false;
			static_cast<AssetReference*>(value)->Guid = guid;
			return true;
		}

		template <typename T>
		const AssetGuid* ReadAssetReferenceGuid(const void* value) noexcept {
			return value ? &static_cast<const T*>(value)->Reference.Guid : nullptr;
		}

		template <typename T>
		bool WriteAssetReferenceGuid(void* value, const AssetGuid& guid) {
			if (!value) return false;
			static_cast<T*>(value)->Reference.Guid = guid;
			return true;
		}
	}

	[[nodiscard]] inline const AssetReferenceCapability* FindAssetReferenceCapability(
		Refl::TypeGuid valueTypeGuid) noexcept {
		static constexpr std::array capabilities{
			AssetReferenceCapability{AssetReferenceTypeGuids::Base, AssetKind::Unknown,
				&AssetReferenceDetail::ReadBaseGuid, &AssetReferenceDetail::WriteBaseGuid},
			AssetReferenceCapability{AssetReferenceTypeGuids::Mesh, AssetKind::Mesh,
				&AssetReferenceDetail::ReadAssetReferenceGuid<MeshAssetRef>, &AssetReferenceDetail::WriteAssetReferenceGuid<MeshAssetRef>},
			AssetReferenceCapability{AssetReferenceTypeGuids::Material, AssetKind::Material,
				&AssetReferenceDetail::ReadAssetReferenceGuid<MaterialAssetRef>, &AssetReferenceDetail::WriteAssetReferenceGuid<MaterialAssetRef>},
			AssetReferenceCapability{AssetReferenceTypeGuids::Texture, AssetKind::Texture2D,
				&AssetReferenceDetail::ReadAssetReferenceGuid<TextureAssetRef>, &AssetReferenceDetail::WriteAssetReferenceGuid<TextureAssetRef>},
			AssetReferenceCapability{AssetReferenceTypeGuids::Shader, AssetKind::Shader,
				&AssetReferenceDetail::ReadAssetReferenceGuid<ShaderAssetRef>, &AssetReferenceDetail::WriteAssetReferenceGuid<ShaderAssetRef>},
		};
		for (const auto& capability : capabilities) {
			if (capability.ValueTypeGuid == valueTypeGuid) return &capability;
		}
		return nullptr;
	}
}
