#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace HE {
	using AssetGuid = std::string;
	using AssetHandle = uint64_t;

	enum class AssetKind {
		Unknown,
		Mesh,
		Material,
		Texture2D,
		Shader
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

	struct AssetReference {
		AssetGuid Guid;

		[[nodiscard]] bool IsValid() const { return !Guid.empty(); }
	};

	struct MeshAssetRef {
		AssetReference Reference;
	};

	struct MaterialAssetRef {
		AssetReference Reference;
	};

	struct TextureAssetRef {
		AssetReference Reference;
	};

	struct ShaderAssetRef {
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
