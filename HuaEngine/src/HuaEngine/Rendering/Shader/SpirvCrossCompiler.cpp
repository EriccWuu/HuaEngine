#include "enginepch.h"
#include "SpirvCrossCompiler.h"

#include <mutex>

#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <spirv_glsl.hpp>

namespace {
	std::once_flag GlslangInitialization;

	HE::ResultEnvelope Failure(std::string operation, std::string message) {
		auto result = HE::ResultEnvelope::Failure(std::move(operation), "shader-stage", message);
		result.AddDetail({ HE::DiagnosticSeverity::Error, "shader.glsl.invalid", std::move(message), "shader-stage" });
		return result;
	}
}

namespace HE::Rendering {
	ResultEnvelope CompileSpirvToOpenGlGlsl(const std::vector<uint32_t>& spirv, ShaderStage stage, SpirvCrossOutput& output) {
		output = {};
		if (spirv.size() < 5 || spirv.front() != 0x07230203u) return Failure("shader.spirv_cross.compile", "SPIR-V module is invalid");
		try {
			spirv_cross::CompilerGLSL compiler(spirv);
			const auto resources = compiler.get_shader_resources();
			const auto& varyings = stage == ShaderStage::Vertex ? resources.stage_outputs : resources.stage_inputs;
			for (const auto& varying : varyings) {
				if (!compiler.has_decoration(varying.id, spv::DecorationLocation)) continue;
				compiler.set_name(varying.id, "varying_location_" + std::to_string(compiler.get_decoration(varying.id, spv::DecorationLocation)));
			}
			compiler.build_combined_image_samplers();
			for (const auto& combined : compiler.get_combined_image_samplers()) {
				const auto textureName = compiler.get_name(combined.image_id);
				const auto samplerName = compiler.get_name(combined.sampler_id);
				const auto uniformName = "SPIRV_Cross_Combined" + textureName + samplerName;
				compiler.set_name(combined.combined_id, uniformName);
				output.CombinedSamplers.push_back({ textureName, samplerName, uniformName });
			}
			spirv_cross::CompilerGLSL::Options options;
			options.version = 330;
			options.es = false;
			options.enable_420pack_extension = false;
			options.vulkan_semantics = false;
			options.emit_uniform_buffer_as_plain_uniforms = false;
			options.vertex.fixup_clipspace = false;
			compiler.set_common_options(options);
			output.Glsl = compiler.compile();
			output.CompilerIdentity = "SPIRV-Cross|vulkan-sdk-1.4.357.0|6c09849fe88c48eaed08413aa022aaa136a3a057|ubo|varying-location-names";
			std::sort(output.CombinedSamplers.begin(), output.CombinedSamplers.end(), [](const auto& a, const auto& b) { return a.UniformName < b.UniformName; });
			return ValidateOpenGlGlsl(output.Glsl, stage);
		}
		catch (const spirv_cross::CompilerError& error) {
			return Failure("shader.spirv_cross.compile", error.what());
		}
	}

	ResultEnvelope ValidateOpenGlGlsl(std::string_view glsl, ShaderStage stage) {
		if (glsl.empty()) return Failure("shader.glsl.validate", "Generated GLSL is empty");
		std::call_once(GlslangInitialization, [] { glslang::InitializeProcess(); });
		const EShLanguage language = stage == ShaderStage::Vertex ? EShLangVertex : EShLangFragment;
		glslang::TShader shader(language);
		const char* source = glsl.data();
		const int length = static_cast<int>(glsl.size());
		shader.setStringsWithLengths(&source, &length, 1);
		shader.setEnvInput(glslang::EShSourceGlsl, language, glslang::EShClientOpenGL, 330);
		shader.setAutoMapLocations(true);
		const auto messages = static_cast<EShMessages>(EShMsgDefault | EShMsgCascadingErrors);
		if (!shader.parse(GetDefaultResources(), 330, false, messages)) {
			return Failure("shader.glsl.validate", std::string(shader.getInfoLog()) + shader.getInfoDebugLog());
		}
		return ResultEnvelope::Success("shader.glsl.validate", "shader-stage", "Generated OpenGL GLSL validated");
	}
}
