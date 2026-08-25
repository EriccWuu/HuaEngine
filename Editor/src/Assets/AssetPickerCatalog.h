#pragma once

#include <span>
#include <vector>

#include "Assets/AssetPickerModel.h"

namespace HE::Editor {
	class AssetPickerCatalog {
	public:
		void Rebuild(std::span<const AssetRecord> records);
		void Clear();
		[[nodiscard]] std::span<const AssetPickerOption> Get(AssetKind kind) const;

	private:
		std::vector<AssetPickerOption> m_MeshOptions;
		std::vector<AssetPickerOption> m_MaterialOptions;
		std::vector<AssetPickerOption> m_TextureOptions;
		std::vector<AssetPickerOption> m_ShaderOptions;
	};
}
