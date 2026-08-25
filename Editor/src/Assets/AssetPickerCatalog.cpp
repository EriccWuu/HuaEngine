#include "enginepch.h"
#include "Assets/AssetPickerCatalog.h"

namespace HE::Editor {
	void AssetPickerCatalog::Rebuild(std::span<const AssetRecord> records) {
		m_MeshOptions = BuildAssetPickerOptions(records, AssetKind::Mesh);
		m_MaterialOptions = BuildAssetPickerOptions(records, AssetKind::Material);
		m_TextureOptions = BuildAssetPickerOptions(records, AssetKind::Texture2D);
		m_ShaderOptions = BuildAssetPickerOptions(records, AssetKind::Shader);
	}

	void AssetPickerCatalog::Clear() {
		m_MeshOptions.clear();
		m_MaterialOptions.clear();
		m_TextureOptions.clear();
		m_ShaderOptions.clear();
	}

	std::span<const AssetPickerOption> AssetPickerCatalog::Get(AssetKind kind) const {
		switch (kind) {
		case AssetKind::Mesh: return m_MeshOptions;
		case AssetKind::Material: return m_MaterialOptions;
		case AssetKind::Texture2D: return m_TextureOptions;
		case AssetKind::Shader: return m_ShaderOptions;
		default: return {};
		}
	}
}
