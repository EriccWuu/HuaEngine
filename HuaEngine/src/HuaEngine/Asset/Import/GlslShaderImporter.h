#pragma once

#include "AssetImporter.h"
#include "HuaEngine/Asset/Artifact/ShaderArtifact.h"

namespace HE {
	class GlslShaderImporter final : public AssetImporter {
	public:
		[[nodiscard]] std::string_view GetId() const override { return "hua.shader-glsl"; }
		[[nodiscard]] uint32_t GetVersion() const override { return 1; }
		[[nodiscard]] uint32_t GetArtifactVersion() const override { return ShaderArtifactVersion; }
		[[nodiscard]] bool CanImport(AssetKind kind, std::string_view extension) const override;
		[[nodiscard]] AssetImportResult Import(const AssetImportContext& context) const override;
	};
}
