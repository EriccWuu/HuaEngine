#pragma once

#include "HuaEngine/Asset/Metadata/AssetImportSettings.h"

namespace HE {
	enum class MeshAxis { PositiveX, NegativeX, PositiveY, NegativeY, PositiveZ, NegativeZ };

	struct ObjMeshImportSettings final : AssetImportSettings {
		float ImportScale = 1.0f;
		MeshAxis UpAxis = MeshAxis::PositiveY;
		MeshAxis ForwardAxis = MeshAxis::NegativeZ;
		bool FlipUvV = false;
		bool GenerateNormalsWhenMissing = true;
		bool RecalculateNormals = false;
		bool ReverseWinding = false;

		[[nodiscard]] std::string_view GetImporterId() const override { return "hua.mesh-obj"; }
		[[nodiscard]] bool operator==(const ObjMeshImportSettings& other) const {
			return ImportScale == other.ImportScale && UpAxis == other.UpAxis && ForwardAxis == other.ForwardAxis && FlipUvV == other.FlipUvV &&
				GenerateNormalsWhenMissing == other.GenerateNormalsWhenMissing && RecalculateNormals == other.RecalculateNormals && ReverseWinding == other.ReverseWinding;
		}
	};
}
