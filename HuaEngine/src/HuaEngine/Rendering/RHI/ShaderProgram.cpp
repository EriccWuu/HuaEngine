#include "enginepch.h"
#include "ShaderProgram.h"

namespace HE::Rendering {
	ResultEnvelope BuildShaderProgramDesc(
		std::vector<ShaderStageBinary> stages,
		ShaderGpuInterface gpuInterface,
		ShaderProgramDesc& output) {
		output = {};
		if (stages.empty()) {
			return ResultEnvelope::Failure("shader.program_desc", "shader-program", "Shader stages must not be empty");
		}
		std::vector<ShaderStage> declaredStages;
		for (const auto& stage : stages) {
			if (stage.Format == ShaderStageCodeFormat::Unknown || stage.EntryPoint.empty() || stage.Code.empty() ||
				std::find(declaredStages.begin(), declaredStages.end(), stage.Stage) != declaredStages.end()) {
				return ResultEnvelope::Failure("shader.program_desc", "shader-program", "Shader stage description is invalid");
			}
			declaredStages.push_back(stage.Stage);
		}
		if (gpuInterface.Stages.empty()) {
			for (const auto& stage : stages) {
				gpuInterface.Stages.push_back({ stage.Stage, stage.EntryPoint });
			}
		}
		ShaderInterface shaderInterface;
		shaderInterface.Gpu = std::move(gpuInterface);
		auto finalizeResult = FinalizeShaderInterface(shaderInterface);
		if (!finalizeResult.Succeeded()) return finalizeResult;
		for (const auto& stage : stages) {
			const auto interfaceStage = std::find_if(shaderInterface.Gpu.Stages.begin(), shaderInterface.Gpu.Stages.end(), [&](const auto& candidate) {
				return candidate.Stage == stage.Stage;
			});
			if (interfaceStage == shaderInterface.Gpu.Stages.end()) {
				return ResultEnvelope::Failure("shader.program_desc", "shader-program", "Shader stages do not match the GPU interface");
			}
		}
		output.Stages = std::move(stages);
		output.Interface = std::move(shaderInterface.Gpu);
		return ResultEnvelope::Success("shader.program_desc", "shader-program", "Shader program description built");
	}
}
