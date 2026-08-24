#include "enginepch.h"
#include "ShaderProgram.h"

namespace HE::Rendering {
	ResultEnvelope BuildOpenGlShaderProgramDesc(
		std::string_view vertexSource,
		std::string_view fragmentSource,
		ShaderGpuInterface gpuInterface,
		ShaderResourceMap resourceMap,
		ShaderProgramDesc& output) {
		output = {};
		if (vertexSource.empty() || fragmentSource.empty()) {
			return ResultEnvelope::Failure("shader.program_desc", "OpenGL", "OpenGL shader stages must not be empty");
		}
		if (gpuInterface.Stages.empty()) {
			gpuInterface.Stages = {
				{ ShaderStage::Vertex, "main" },
				{ ShaderStage::Fragment, "main" }
			};
		}
		ShaderInterface shaderInterface;
		shaderInterface.Gpu = std::move(gpuInterface);
		auto finalizeResult = FinalizeShaderInterface(shaderInterface);
		if (!finalizeResult.Succeeded()) return finalizeResult;
		output.Stages = {
			{ ShaderStage::Vertex, ShaderStageCodeFormat::OpenGlGlsl, "main", { vertexSource.begin(), vertexSource.end() } },
			{ ShaderStage::Fragment, ShaderStageCodeFormat::OpenGlGlsl, "main", { fragmentSource.begin(), fragmentSource.end() } }
		};
		output.Interface = std::move(shaderInterface.Gpu);
		output.ResourceMap = std::move(resourceMap);
		return ResultEnvelope::Success("shader.program_desc", "OpenGL", "OpenGL shader program description built");
	}
}
