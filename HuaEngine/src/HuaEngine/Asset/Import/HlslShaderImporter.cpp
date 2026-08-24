#include "enginepch.h"
#include "HlslShaderImporter.h"

#include <fstream>
#include <regex>
#include <set>

#include "HuaEngine/Asset/Import/AssetSourceHash.h"
#include "HuaEngine/Asset/Import/ShaderDescriptor.h"
#include "HuaEngine/Asset/BuiltinAssetCatalog.h"
#include "HuaEngine/Rendering/Shader/DxcShaderCompiler.h"
#include "HuaEngine/Rendering/Shader/SpirvShaderReflector.h"
#include "HuaEngine/Rendering/Shader/SpirvCrossCompiler.h"

namespace {
	using namespace HE;

	ResultEnvelope Failure(const std::filesystem::path& path, std::string message) {
		return ResultEnvelope::Failure("asset.shader_inputs", path.generic_string(), std::move(message));
	}

	bool IsWithin(const std::filesystem::path& root, const std::filesystem::path& path) {
		const auto relative = path.lexically_relative(root);
		return !relative.empty() && !relative.is_absolute() && std::none_of(relative.begin(), relative.end(), [](const auto& part) { return part == ".."; });
	}

	ResultEnvelope CollectInputs(
		const AssetImportContext& context,
		const ShaderDescriptor& descriptor,
		std::string_view rootSourceHash,
		std::vector<AssetImportSourceInput>& output,
		std::filesystem::path& hlslPath,
		std::vector<std::filesystem::path>& includeRoots) {
		output.clear();
		includeRoots.clear();
		const auto sourceRoot = context.SourceAsset.Source == AssetSource::Builtin ? GetBuiltinAssetRootPath() : context.Project.GetAssetRootPath();
		const auto assetRoot = std::filesystem::weakly_canonical(sourceRoot);
		const auto descriptorPath = std::filesystem::weakly_canonical(context.SourcePath);
		hlslPath = std::filesystem::weakly_canonical(descriptorPath.parent_path() / descriptor.Source);
		if (!IsWithin(assetRoot, descriptorPath) || !IsWithin(assetRoot, hlslPath) || !std::filesystem::is_regular_file(hlslPath)) return Failure(context.SourcePath, "Shader source path escapes Assets or does not exist");
		includeRoots = { hlslPath.parent_path(), assetRoot };
		std::sort(includeRoots.begin(), includeRoots.end());
		includeRoots.erase(std::unique(includeRoots.begin(), includeRoots.end()), includeRoots.end());
		output.push_back({ descriptorPath.lexically_relative(assetRoot).generic_string(), std::string(rootSourceHash) });
		std::set<std::filesystem::path> visited;
		std::vector<std::filesystem::path> pending = { hlslPath };
		const std::regex includePattern(R"(^\s*#\s*include\s*[\"<]([^\">]+)[\">])");
		while (!pending.empty()) {
			const auto current = pending.back(); pending.pop_back();
			if (!visited.emplace(current).second) continue;
			if (!IsWithin(assetRoot, current) || !std::filesystem::is_regular_file(current)) return Failure(current, "Shader include escapes Assets or does not exist");
			std::string hash;
			auto hashResult = ComputeAssetSourceHash(current, hash);
			if (!hashResult.Succeeded()) return hashResult;
			output.push_back({ current.lexically_relative(assetRoot).generic_string(), std::move(hash) });
			std::ifstream stream(current);
			std::string line;
			while (std::getline(stream, line)) {
				std::smatch match;
				if (!std::regex_search(line, match, includePattern)) continue;
				const std::filesystem::path includePath(match[1].str());
				if (includePath.is_absolute() || includePath.has_root_name()) return Failure(current, "Absolute shader include path is forbidden");
				std::filesystem::path resolved;
				std::vector<std::filesystem::path> searchRoots = { current.parent_path() };
				searchRoots.insert(searchRoots.end(), includeRoots.begin(), includeRoots.end());
				for (const auto& root : searchRoots) {
					const auto candidate = std::filesystem::weakly_canonical(root / includePath);
					if (IsWithin(assetRoot, candidate) && std::filesystem::is_regular_file(candidate)) {
						resolved = candidate;
						break;
					}
				}
				if (resolved.empty()) return Failure(current, "Shader include does not resolve through the compiler include roots");
				pending.push_back(resolved);
			}
		}
		std::sort(output.begin(), output.end(), [](const auto& a, const auto& b) { return a.NormalizedPath < b.NormalizedPath; });
		return ResultEnvelope::Success("asset.shader_inputs", context.SourceAsset.Guid, "Shader source inputs collected");
	}

	void AppendDiagnostics(const ResultEnvelope& source, AssetImportResult& destination) {
		if (source.Details.empty()) destination.Diagnostics.push_back({ DiagnosticSeverity::Error, "asset.import.shader_failed", source.Summary, source.Target });
		else destination.Diagnostics.insert(destination.Diagnostics.end(), source.Details.begin(), source.Details.end());
	}

	bool HasPortableConstantLayout(const Rendering::ShaderConstantBuffer& buffer) {
		uint32_t offset = 0;
		for (const auto& member : buffer.Members) {
			const bool matrix = member.Type == Rendering::ShaderValueType::Float4x4;
			if (matrix || member.Size > 16 - (offset % 16)) offset = (offset + 15u) & ~15u;
			if (member.Offset != offset) return false;
			offset += member.Size;
		}
		return true;
	}
}

namespace HE {
	bool HlslShaderImporter::CanImport(AssetKind kind, std::string_view extension) const { return kind == AssetKind::Shader && extension == ".shader"; }

	ResultEnvelope HlslShaderImporter::BuildFingerprintInput(const AssetImportContext& context, std::string_view rootSourceHash, AssetImportFingerprintInput& output) const {
		ShaderDescriptor descriptor;
		auto descriptorResult = LoadShaderDescriptor(context.SourcePath, descriptor);
		if (!descriptorResult.Succeeded()) return descriptorResult;
		std::filesystem::path hlslPath;
		std::vector<std::filesystem::path> includeRoots;
		std::vector<AssetImportSourceInput> sources;
		auto inputsResult = CollectInputs(context, descriptor, rootSourceHash, sources, hlslPath, includeRoots);
		if (!inputsResult.Succeeded()) return inputsResult;
		Rendering::DxcShaderCompiler compiler;
		std::string compilerIdentity;
		auto identityResult = compiler.QueryCompilerIdentity(compilerIdentity);
		if (!identityResult.Succeeded()) return identityResult;
		std::string compileOptions = "-spirv|-fspv-target-env=vulkan1.2|-fspv-reflect|-fvk-use-gl-layout|-Zpc|-WX|-Ges";
#ifdef _DEBUG
		compileOptions += "|-Zi|-Od|-fspv-debug=vulkan-with-source";
#else
		compileOptions += "|-O3|-Qstrip_debug";
#endif
		output = { .ImporterId = std::string(GetId()), .ImporterVersion = GetVersion(), .ArtifactVersion = GetArtifactVersion(), .Sources = std::move(sources),
			.Options = { { "compiler", std::move(compilerIdentity) }, { "spirv_cross", "vulkan-sdk-1.4.357.0|6c09849fe88c48eaed08413aa022aaa136a3a057|ubo" }, { "glslang", "16.5.0" }, { "opengl_glsl", "330" }, { "vertex", descriptor.Vertex.Entry + "|" + descriptor.Vertex.Profile }, { "fragment", descriptor.Fragment.Entry + "|" + descriptor.Fragment.Profile }, { "options", std::move(compileOptions) } } };
		return ResultEnvelope::Success("asset.import_fingerprint.inputs", context.SourceAsset.Guid, "Shader fingerprint inputs collected");
	}

	AssetImportResult HlslShaderImporter::Import(const AssetImportContext& context) const {
		AssetImportResult result;
		ShaderDescriptor descriptor;
		auto descriptorResult = LoadShaderDescriptor(context.SourcePath, descriptor);
		if (!descriptorResult.Succeeded()) { AppendDiagnostics(descriptorResult, result); return result; }
		std::string descriptorHash;
		if (!ComputeAssetSourceHash(context.SourcePath, descriptorHash).Succeeded()) return result;
		std::filesystem::path hlslPath;
		std::vector<std::filesystem::path> includeRoots;
		std::vector<AssetImportSourceInput> inputs;
		auto inputsResult = CollectInputs(context, descriptor, descriptorHash, inputs, hlslPath, includeRoots);
		if (!inputsResult.Succeeded()) { AppendDiagnostics(inputsResult, result); return result; }
		Rendering::DxcShaderCompiler compiler;
		Rendering::DxcCompileOutput vertexOutput, fragmentOutput;
		auto vertexResult = compiler.Compile({ hlslPath, descriptor.Vertex.Entry, descriptor.Vertex.Profile, Rendering::ShaderStage::Vertex, includeRoots }, vertexOutput);
		if (!vertexResult.Succeeded()) { AppendDiagnostics(vertexResult, result); return result; }
		auto fragmentResult = compiler.Compile({ hlslPath, descriptor.Fragment.Entry, descriptor.Fragment.Profile, Rendering::ShaderStage::Fragment, includeRoots }, fragmentOutput);
		if (!fragmentResult.Succeeded()) { AppendDiagnostics(fragmentResult, result); return result; }
		Rendering::ShaderGpuInterface vertexInterface, fragmentInterface;
		auto reflectResult = Rendering::ReflectSpirvAssembly(vertexOutput.Disassembly, Rendering::ShaderStage::Vertex, descriptor.Vertex.Entry, vertexInterface);
		if (!reflectResult.Succeeded()) { AppendDiagnostics(reflectResult, result); return result; }
		reflectResult = Rendering::ReflectSpirvAssembly(fragmentOutput.Disassembly, Rendering::ShaderStage::Fragment, descriptor.Fragment.Entry, fragmentInterface);
		if (!reflectResult.Succeeded()) { AppendDiagnostics(reflectResult, result); return result; }
		Rendering::ShaderInterface shaderInterface;
		auto mergeResult = Rendering::MergeShaderStageInterfaces(vertexInterface, fragmentInterface, shaderInterface.Gpu);
		if (!mergeResult.Succeeded()) { AppendDiagnostics(mergeResult, result); return result; }
		if (!std::all_of(shaderInterface.Gpu.ConstantBuffers.begin(), shaderInterface.Gpu.ConstantBuffers.end(), HasPortableConstantLayout)) {
			result.Diagnostics.push_back({ DiagnosticSeverity::Error, "asset.import.shader_layout_incompatible", "Constant buffer layout differs between HLSL register packing and OpenGL std140; add explicit padding", context.SourcePath.generic_string() });
			return result;
		}
		for (const auto& parameter : descriptor.Parameters) if (parameter.Scope == Rendering::ShaderParameterScope::Material) shaderInterface.Authoring.Parameters.push_back(parameter);
		for (const auto& parameter : descriptor.Parameters) {
			if (parameter.Scope != Rendering::ShaderParameterScope::Material) continue;
			bool found = false;
			for (const auto& buffer : shaderInterface.Gpu.ConstantBuffers) if (buffer.Set == 1) found |= std::any_of(buffer.Members.begin(), buffer.Members.end(), [&](const auto& member) { return member.Name == parameter.Name && member.Type == parameter.Type; });
			for (const auto& resource : shaderInterface.Gpu.Resources) if (resource.Set == 1) found |= resource.Name == parameter.Name && parameter.Type == Rendering::ShaderValueType::Texture2D;
			if (!found) { result.Diagnostics.push_back({ DiagnosticSeverity::Error, "asset.import.shader_parameter_missing", "Exposed material parameter is missing or has an incompatible reflected type", parameter.Name }); return result; }
		}
		auto finalizeResult = Rendering::FinalizeShaderInterface(shaderInterface);
		if (!finalizeResult.Succeeded()) { AppendDiagnostics(finalizeResult, result); return result; }
		ShaderArtifactDataV2 artifactData;
		Rendering::SpirvCrossOutput vertexGlsl, fragmentGlsl;
		auto crossResult = Rendering::CompileSpirvToOpenGlGlsl(vertexOutput.Spirv, Rendering::ShaderStage::Vertex, vertexGlsl);
		if (!crossResult.Succeeded()) { AppendDiagnostics(crossResult, result); return result; }
		crossResult = Rendering::CompileSpirvToOpenGlGlsl(fragmentOutput.Spirv, Rendering::ShaderStage::Fragment, fragmentGlsl);
		if (!crossResult.Succeeded()) { AppendDiagnostics(crossResult, result); return result; }
		artifactData.CompilerIdentity = vertexOutput.CompilerIdentity + "|" + vertexGlsl.CompilerIdentity + "|glslang-16.5.0";
		artifactData.CompileOptions = vertexOutput.CompileOptions;
		artifactData.Stages = { { Rendering::ShaderStage::Vertex, descriptor.Vertex.Entry, descriptor.Vertex.Profile, std::move(vertexOutput.Spirv), std::move(vertexGlsl.Glsl) }, { Rendering::ShaderStage::Fragment, descriptor.Fragment.Entry, descriptor.Fragment.Profile, std::move(fragmentOutput.Spirv), std::move(fragmentGlsl.Glsl) } };
		artifactData.Interface = std::move(shaderInterface);
		artifactData.OpenGlCombinedSamplers = std::move(vertexGlsl.CombinedSamplers);
		artifactData.OpenGlCombinedSamplers.insert(artifactData.OpenGlCombinedSamplers.end(), fragmentGlsl.CombinedSamplers.begin(), fragmentGlsl.CombinedSamplers.end());
		artifactData.ImportInputs = std::move(inputs);
		auto encodeResult = EncodeShaderArtifactV2(artifactData, result.Artifact);
		if (!encodeResult.Succeeded()) { AppendDiagnostics(encodeResult, result); return result; }
		result.Success = true;
		return result;
	}
}
