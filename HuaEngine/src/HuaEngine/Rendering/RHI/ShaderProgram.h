#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "HuaEngine/Core/ResultEnvelope.h"
#include "HuaEngine/Rendering/Shader/ShaderInterface.h"

namespace HE::Rendering {
	enum class ShaderStageCodeFormat : uint8_t {
		Unknown = 0,
		OpenGlGlsl,
		SpirV,
		Dxil
	};

	struct ShaderStageBinary {
		ShaderStage Stage = ShaderStage::Vertex;
		ShaderStageCodeFormat Format = ShaderStageCodeFormat::Unknown;
		std::string EntryPoint;
		std::vector<uint8_t> Code;
	};

	struct ShaderProgramDesc {
		std::vector<ShaderStageBinary> Stages;
		ShaderGpuInterface Interface;
	};

	ResultEnvelope BuildShaderProgramDesc(
		std::vector<ShaderStageBinary> stages,
		ShaderGpuInterface gpuInterface,
		ShaderProgramDesc& output);

	class ShaderProgram {
	public:
		virtual ~ShaderProgram() = default;

		virtual const ShaderProgramDesc& GetDesc() const = 0;
	};
}
