#pragma once

#include <string_view>

#include "HuaEngine/Core/ResultEnvelope.h"
#include "HuaEngine/Rendering/Shader/ShaderInterface.h"

namespace HE::Rendering {
	ResultEnvelope ReflectSpirvAssembly(
		std::string_view assembly,
		ShaderStage stage,
		std::string_view entryPoint,
		ShaderGpuInterface& output);

	ResultEnvelope MergeShaderStageInterfaces(
		const ShaderGpuInterface& vertex,
		const ShaderGpuInterface& fragment,
		ShaderGpuInterface& output);
}
