#pragma once

#include "HuaEngine/Rendering/Material/MaterialSourceData.h"

namespace HE::Editor {
	class AssetPreviewContext {
	public:
		void SetMaterial(const Rendering::MaterialSourceData& material) { m_Material = material; }
		void DrawMaterialPreview() const;
	private:
		Rendering::MaterialSourceData m_Material;
	};
}
