#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Rendering/Material/MaterialCore.h"
#include "HuaEngine/Rendering/RHI/TextureResource.h"

namespace HE::Rendering {
	struct MaterialParameterValueResolved {
		std::string Name;
		MaterialParameterType Type = MaterialParameterType::Float;
		MaterialParameterValue Value = 0.0f;
	};

	struct MaterialTextureBinding {
		std::string Name;
		uint32_t Slot = 0;
		Ref<TextureResource> Texture;
	};

	struct MaterialBinding {
		std::vector<MaterialParameterValueResolved> Parameters;
		std::vector<MaterialTextureBinding> Textures;
	};
}
