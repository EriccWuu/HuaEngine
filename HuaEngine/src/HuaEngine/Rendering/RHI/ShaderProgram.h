#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "HuaEngine/Core/Sha256.h"

namespace HE::Rendering {
	struct ShaderUniformMemberBinding {
		std::string Name;
		uint32_t Offset = 0;
		uint32_t Size = 0;
	};

	struct ShaderUniformBlockBinding {
		std::string Name;
		uint32_t Set = 0;
		uint32_t Binding = 0;
		uint32_t BindingPoint = 0;
		uint32_t Size = 0;
		std::vector<ShaderUniformMemberBinding> Members;
	};

	struct ShaderTextureBinding {
		std::string TextureName;
		std::string SamplerName;
		std::string UniformName;
		uint32_t TextureUnit = 0;
	};

	struct ShaderProgramDesc {
		std::string VertexSource;
		std::string FragmentSource;
		std::vector<ShaderUniformBlockBinding> UniformBlocks;
		std::vector<ShaderTextureBinding> Textures;
		Sha256Digest InterfaceDigest{};
		uint64_t InterfaceSignature = 0;
	};

	class ShaderProgram {
	public:
		virtual ~ShaderProgram() = default;

		virtual const ShaderProgramDesc& GetDesc() const = 0;
	};
}
