#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "HuaEngine/Core/ResultEnvelope.h"
#include "HuaEngine/Rendering/Shader/ShaderInterface.h"

namespace HE::Rendering {
	enum class ShaderStageCodeFormat : uint8_t {
		OpenGlGlsl = 0,
		SpirV,
		Dxil
	};

	struct ShaderStageBinary {
		ShaderStage Stage = ShaderStage::Vertex;
		ShaderStageCodeFormat Format = ShaderStageCodeFormat::OpenGlGlsl;
		std::string EntryPoint;
		std::vector<uint8_t> Code;
	};

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
		uint8_t StageMask = 0;
		std::vector<ShaderUniformMemberBinding> Members;
	};

	struct ShaderTextureBinding {
		std::string TextureName;
		std::string SamplerName;
		std::string UniformName;
		uint32_t TextureSet = 0;
		uint32_t TextureBinding = 0;
		uint32_t SamplerSet = 0;
		uint32_t SamplerBinding = 0;
		uint32_t TextureUnit = 0;
		uint8_t StageMask = 0;
	};

	struct ShaderResourceMap {
		std::vector<ShaderUniformBlockBinding> UniformBlocks;
		std::vector<ShaderTextureBinding> Textures;
	};

	struct ShaderProgramDesc {
		std::vector<ShaderStageBinary> Stages;
		ShaderGpuInterface Interface;
		ShaderResourceMap ResourceMap;
	};

	ResultEnvelope BuildOpenGlShaderProgramDesc(
		std::string_view vertexSource,
		std::string_view fragmentSource,
		ShaderGpuInterface gpuInterface,
		ShaderResourceMap resourceMap,
		ShaderProgramDesc& output);

	class ShaderProgram {
	public:
		virtual ~ShaderProgram() = default;

		virtual const ShaderProgramDesc& GetDesc() const = 0;
	};
}
