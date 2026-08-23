#pragma once

#include "AssetImporter.h"
#include "HuaEngine/Asset/Artifact/MeshArtifact.h"

namespace HE {
	class ObjMeshImporter final : public AssetImporter {
	public:
		[[nodiscard]] std::string_view GetId() const override { return "hua.mesh-obj"; }
		[[nodiscard]] uint32_t GetVersion() const override { return 1; }
		[[nodiscard]] uint32_t GetArtifactVersion() const override { return MeshArtifactVersion; }
		[[nodiscard]] bool CanImport(AssetKind kind, std::string_view extension) const override;
		[[nodiscard]] AssetImportResult Import(const AssetImportContext& context) const override;
	};
}
