#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "HuaEngine/Reflection/ReflectionMarkers.h"
#include "HuaEngine/Reflection/TypeGuid.h"

namespace HE {
	using AssetGuid = std::string;
	using AssetHandle = uint64_t;

	enum class AssetKind {
		Unknown,
		Mesh,
		Material,
		Texture2D,
		Shader,
		Scene
	};

	enum class AssetSource {
		Unknown,
		File,
		Builtin
	};

	enum class AssetImportState {
		Unknown,
		Imported,
		Registered,
		Builtin,
		Missing
	};

	enum class BuiltinMeshPrimitive {
		Quad,
		Cube,
		Sphere
	};

	namespace AssetReferenceTypeGuids {
		inline constexpr Refl::TypeGuid Base{0x4f745e86ab69460dULL, 0xb41dac991f79c006ULL};
		inline constexpr Refl::TypeGuid Mesh{0x4f745e86ab69460dULL, 0xb41dac991f79c007ULL};
		inline constexpr Refl::TypeGuid Material{0x4f745e86ab69460dULL, 0xb41dac991f79c008ULL};
		inline constexpr Refl::TypeGuid Texture{0x4f745e86ab69460dULL, 0xb41dac991f79c009ULL};
		inline constexpr Refl::TypeGuid Shader{0x4f745e86ab69460dULL, 0xb41dac991f79c00aULL};
	}

	struct [[sattr(guid="4f745e86ab69460db41dac991f79c006"; reflect=@marked)]] AssetReference {
		AssetGuid Guid;

		[[nodiscard]] bool IsValid() const { return !Guid.empty(); }
	};

	struct [[sattr(guid="4f745e86ab69460db41dac991f79c007"; reflect=@marked)]] MeshAssetRef {
		AssetReference Reference;
	};

	struct [[sattr(guid="4f745e86ab69460db41dac991f79c008"; reflect=@marked)]] MaterialAssetRef {
		AssetReference Reference;
	};

	struct [[sattr(guid="4f745e86ab69460db41dac991f79c009"; reflect=@marked)]] TextureAssetRef {
		AssetReference Reference;
	};

	struct [[sattr(guid="4f745e86ab69460db41dac991f79c00a"; reflect=@marked)]] ShaderAssetRef {
		AssetReference Reference;
	};

	namespace BuiltinAssetGuids {
		inline const AssetGuid QuadMesh = "builtin-mesh-quad";
		inline const AssetGuid CubeMesh = "builtin-mesh-cube";
		inline const AssetGuid SphereMesh = "builtin-mesh-sphere";
		inline const AssetGuid DefaultMaterial = "builtin-material-default";
		inline const AssetGuid FallbackMesh = "builtin-mesh-fallback";
		inline const AssetGuid FallbackMaterial = "builtin-material-fallback";
		inline const AssetGuid UnlitColorShader = "builtin-shader-unlit-color";
	}

	std::string GenerateAssetGuid();
	std::string_view ToString(AssetKind kind);
	std::string_view ToString(AssetSource source);
	std::string_view ToString(AssetImportState state);
	std::string_view ToString(BuiltinMeshPrimitive primitive);
	AssetKind AssetKindFromString(std::string_view value);
	AssetSource AssetSourceFromString(std::string_view value);
	AssetImportState AssetImportStateFromString(std::string_view value);
}
