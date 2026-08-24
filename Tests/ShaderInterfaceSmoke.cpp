#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "HuaEngine/Asset/Import/AssetImportFingerprint.h"
#include "HuaEngine/Asset/Import/HlslShaderImporter.h"
#include "HuaEngine/Asset/Import/AssetSourceHash.h"
#include "HuaEngine/Asset/Import/ShaderDescriptor.h"
#include "HuaEngine/Core/Sha256.h"
#include "HuaEngine/Rendering/Material/MaterialDefinition.h"
#include "HuaEngine/Rendering/Shader/DxcShaderCompiler.h"
#include "HuaEngine/Rendering/Shader/ShaderInterface.h"
#include "HuaEngine/Rendering/Shader/SpirvShaderReflector.h"
#include "HuaEngine/Rendering/Shader/SpirvCrossCompiler.h"
#include "Module/Rendering/RenderingComponent.h"

namespace {
	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[ShaderInterfaceSmoke] " << message << std::endl;
			std::exit(1);
		}
	}

	void WriteText(const std::filesystem::path& path, std::string_view text) {
		std::ofstream stream(path, std::ios::out | std::ios::trunc);
		stream << text;
		Require(stream.good(), "Expected fixture write");
	}

	void TestDescriptorRoundTrip(const std::filesystem::path& root) {
		const auto sourcePath = root / "Sandbox.shader";
		const auto savedPath = root / "Saved.shader";
		WriteText(sourcePath,
			"name: Sandbox\n"
			"language: HLSL\n"
			"source: shaders/sandbox.hlsl\n"
			"stages:\n"
			"  vertex: { entry: VSMain, profile: vs_6_0 }\n"
			"  fragment: { entry: PSMain, profile: ps_6_0 }\n"
			"parameters:\n"
			"  u_Color:\n"
			"    display_name: Color\n"
			"    scope: Material\n"
			"    editor: Color\n"
			"    default: [1.0, 0.5, 0.25, 1.0]\n"
			"  u_Texture:\n"
			"    display_name: Texture\n"
			"    scope: Material\n"
			"    editor: Texture2D\n");

		HE::ShaderDescriptor descriptor;
		Require(HE::LoadShaderDescriptor(sourcePath, descriptor).Succeeded(), "Expected legal shader descriptor");
		Require(HE::SaveShaderDescriptor(savedPath, descriptor).Succeeded(), "Expected shader descriptor save");
		HE::ShaderDescriptor reloaded;
		Require(HE::LoadShaderDescriptor(savedPath, reloaded).Succeeded(), "Expected saved shader descriptor reload");
		Require(reloaded.Name == descriptor.Name && reloaded.Source == descriptor.Source && reloaded.Parameters.size() == 2, "Expected stable descriptor round-trip");

		WriteText(sourcePath,
			"name: Escape\nlanguage: HLSL\nsource: ../escape.hlsl\n"
			"stages:\n  vertex: { entry: VSMain, profile: vs_6_0 }\n  fragment: { entry: PSMain, profile: ps_6_0 }\nparameters: {}\n");
		Require(HE::LoadShaderDescriptor(sourcePath, descriptor).Failed(), "Expected escaping source rejection");
		WriteText(sourcePath,
			"name: BadStage\nlanguage: HLSL\nsource: shader.hlsl\n"
			"stages:\n  vertex: { entry: 'bad entry', profile: ps_6_0 }\n  fragment: { entry: PSMain, profile: ps_6_0 }\nparameters: {}\n");
		Require(HE::LoadShaderDescriptor(sourcePath, descriptor).Failed(), "Expected invalid stage rejection");
		WriteText(sourcePath,
			"name: BadScope\nlanguage: HLSL\nsource: shader.hlsl\n"
			"stages:\n  vertex: { entry: VSMain, profile: vs_6_0 }\n  fragment: { entry: PSMain, profile: ps_6_0 }\n"
			"parameters:\n  u_Color: { display_name: Color, scope: Global, editor: Color, default: [1, 1, 1, 1] }\n");
		Require(HE::LoadShaderDescriptor(sourcePath, descriptor).Failed(), "Expected invalid parameter scope rejection");
	}

	HE::Rendering::ShaderInterface MakeInterface(bool reverseOrder) {
		using namespace HE::Rendering;
		ShaderInterface result;
		result.Gpu.Stages = { { ShaderStage::Vertex, "VSMain" }, { ShaderStage::Fragment, "PSMain" } };
		result.Gpu.Resources = {
			{ "MaterialData", ShaderResourceType::ConstantBuffer, 1, 0, 1, 3 },
			{ "u_Texture", ShaderResourceType::Texture2D, 1, 1, 1, 2 }
		};
		result.Gpu.ConstantBuffers = { { "MaterialData", 1, 0, 16, { { "u_Color", ShaderValueType::Float4, 0, 16 } } } };
		result.Authoring.Parameters = { { "u_Color", "Color", ShaderParameterScope::Material, ShaderValueType::Float4, ShaderEditorKind::Color, glm::vec4(1.0f) } };
		if (reverseOrder) {
			std::reverse(result.Gpu.Stages.begin(), result.Gpu.Stages.end());
			std::reverse(result.Gpu.Resources.begin(), result.Gpu.Resources.end());
		}
		return result;
	}

	void TestCanonicalIdentity() {
		auto first = MakeInterface(false);
		auto second = MakeInterface(true);
		Require(HE::Rendering::FinalizeShaderInterface(first).Succeeded(), "Expected interface finalization");
		Require(HE::Rendering::FinalizeShaderInterface(second).Succeeded(), "Expected reordered interface finalization");
		Require(first.Gpu.Digest == second.Gpu.Digest && first.Gpu.Signature == second.Gpu.Signature, "Expected order-independent GPU identity");

		auto duplicate = MakeInterface(false);
		duplicate.Gpu.Resources.push_back({ "Conflict", HE::Rendering::ShaderResourceType::Sampler, 1, 0, 1, 2 });
		Require(HE::Rendering::FinalizeShaderInterface(duplicate).Failed(), "Expected duplicate binding rejection");
		HE::Rendering::MaterialDefinition definition({}, first.Authoring.Digest);
		Require(definition.GetDigest() == first.Authoring.Digest, "Expected immutable material definition identity");
		HE::Rendering::MaterialDefinition overrideDefinition({
			{ .Name = "u_Color", .Type = HE::Rendering::ShaderValueType::Float4 },
			{ .Name = "u_Texture", .Type = HE::Rendering::ShaderValueType::Texture2D }
		}, first.Authoring.Digest);
		HE::Rendering::MaterialOverrideSet overrides;
		overrides.Parameters["u_Color"] = 1.0f;
		overrides.Parameters["u_Unknown"] = glm::vec4(1.0f);
		overrides.TextureParameters["u_Texture"] = "texture-guid";
		Require(HE::Rendering::ReconcileMaterialOverrides(overrides, overrideDefinition), "Expected incompatible material overrides to be removed");
		Require(overrides.Parameters.empty() && overrides.TextureParameters.contains("u_Texture"), "Expected compatible texture override to remain");
		HE::Rendering::ShaderGpuInterface vertexStage;
		HE::Rendering::ShaderGpuInterface fragmentStage;
		vertexStage.Stages = { { HE::Rendering::ShaderStage::Vertex, "VSMain", {}, { { 0, HE::Rendering::ShaderValueType::Float2 } } } };
		fragmentStage.Stages = { { HE::Rendering::ShaderStage::Fragment, "PSMain", { { 0, HE::Rendering::ShaderValueType::Float3 } }, {} } };
		HE::Rendering::ShaderGpuInterface merged;
		Require(HE::Rendering::MergeShaderStageInterfaces(vertexStage, fragmentStage, merged).Failed(), "Expected incompatible stage interface rejection");
	}

	void TestImportFingerprint() {
		constexpr std::string_view HashA = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
		constexpr std::string_view HashB = "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789";
		HE::AssetImportFingerprintInput first{
			.ImporterId = "hua.shader",
			.ImporterVersion = 1,
			.ArtifactVersion = 1,
			.Sources = { { "shaders/main.hlsl", std::string(HashA) }, { "shaders/common.hlsli", std::string(HashB) } },
			.Options = { { "profile", "6_0" }, { "layout", "column-major" } }
		};
		auto second = first;
		std::reverse(second.Sources.begin(), second.Sources.end());
		std::reverse(second.Options.begin(), second.Options.end());
		std::string firstDigest;
		std::string secondDigest;
		Require(HE::ComputeAssetImportFingerprint(first, firstDigest).Succeeded(), "Expected import fingerprint");
		Require(HE::ComputeAssetImportFingerprint(second, secondDigest).Succeeded() && firstDigest == secondDigest, "Expected canonical import fingerprint");
		first.Sources[0].NormalizedPath = "../escape.hlsl";
		Require(HE::ComputeAssetImportFingerprint(first, firstDigest).Failed(), "Expected unsafe fingerprint path rejection");
		Require(HE::Rendering::ValidateOpenGlGlsl("#version 330\nthis is not GLSL", HE::Rendering::ShaderStage::Fragment).Failed(), "Expected invalid generated GLSL rejection");
	}

	void TestHlslImport(const std::filesystem::path& root) {
		const auto assetRoot = root / "Assets";
		const auto shaderRoot = assetRoot / "Shaders";
		std::filesystem::create_directories(shaderRoot);
		std::filesystem::create_directories(assetRoot / "Shared");
		WriteText(shaderRoot / "Common.hlsli", "float4 MakePosition(float3 value) { return float4(value, 1.0); }\n");
		WriteText(assetRoot / "Shared" / "RootInclude.hlsli", "float RootScale() { return 1.0; }\n");
		WriteText(shaderRoot / "Sandbox.hlsl",
			"#include \"Common.hlsli\"\n"
			"#include \"Shared/RootInclude.hlsli\"\n"
			"[[vk::binding(0, 0)]] cbuffer FrameData : register(b0, space0) { float4x4 u_ViewProjection; };\n"
			"[[vk::binding(0, 1)]] cbuffer MaterialData : register(b0, space1) { float u_Roughness; float _Padding0; float2 u_UvScale; float3 u_Emissive; float u_Alpha; float4 u_Color; };\n"
			"[[vk::binding(1, 1)]] Texture2D u_Texture : register(t0, space1); [[vk::binding(2, 1)]] SamplerState u_TextureSampler : register(s0, space1);\n"
			"[[vk::binding(0, 2)]] cbuffer ObjectData : register(b0, space2) { float4x4 u_Transform; };\n"
			"struct VSInput { float3 Position : POSITION; }; struct VSOutput { float4 Position : SV_Position; float2 Uv : TEXCOORD0; };\n"
			"VSOutput VSMain(VSInput input) { VSOutput output; output.Position = mul(u_ViewProjection, mul(u_Transform, MakePosition(input.Position * RootScale()))); output.Uv = input.Position.xy; return output; }\n"
			"float4 PSMain(VSOutput input) : SV_Target0 { return u_Color * u_Texture.Sample(u_TextureSampler, input.Uv) + float4(u_Emissive * u_Roughness, u_Alpha) + float4(u_UvScale, 0, 0); }\n");
		const auto descriptorPath = shaderRoot / "Sandbox.shader";
		WriteText(descriptorPath,
			"name: Sandbox\nlanguage: HLSL\nsource: Sandbox.hlsl\n"
			"stages:\n  vertex: { entry: VSMain, profile: vs_6_0 }\n  fragment: { entry: PSMain, profile: ps_6_0 }\n"
			"parameters:\n  u_Color: { display_name: Color, scope: Material, editor: Color, default: [1, 1, 1, 1] }\n  u_Texture: { display_name: Texture, scope: Material, editor: Texture2D }\n");
		HE::ProjectContext context;
		context.RootPath = root;
		context.ProjectFilePath = root / ".huaengine" / "project.json";
		const HE::AssetManifestRecord record{ .Guid = "shader-guid", .AssetId = "Shaders/Sandbox.shader", .Kind = HE::AssetKind::Shader, .Source = HE::AssetSource::File, .RelativePath = "Shaders/Sandbox.shader", .ImportState = HE::AssetImportState::Registered };
		const HE::AssetImportContext importContext{ context, record, descriptorPath, nullptr };
		HE::HlslShaderImporter importer;
		std::string rootHash;
		Require(HE::ComputeAssetSourceHash(descriptorPath, rootHash).Succeeded(), "Expected descriptor hash");
		HE::AssetImportFingerprintInput firstInputs;
		const auto fingerprintInputsResult = importer.BuildFingerprintInput(importContext, rootHash, firstInputs);
		Require(fingerprintInputsResult.Succeeded() && firstInputs.Sources.size() == 4, "Expected descriptor, HLSL, local include, and asset-root include fingerprint inputs: " + fingerprintInputsResult.Summary);
		std::string firstFingerprint;
		Require(HE::ComputeAssetImportFingerprint(firstInputs, firstFingerprint).Succeeded(), "Expected shader fingerprint");

		const auto importResult = importer.Import(importContext);
		Require(importResult.Success, importResult.Diagnostics.empty() ? "Expected HLSL import" : importResult.Diagnostics.front().Message);
		HE::ShaderArtifactDataV2 artifact;
		Require(HE::DecodeShaderArtifactV2(importResult.Artifact, artifact).Succeeded(), "Expected Shader Artifact V2 round-trip");
		Require(artifact.Stages.size() == 2 && artifact.Stages[0].Spirv.front() == 0x07230203u, "Expected SPIR-V stage artifact");
		HE::AssetArtifact rejectedArtifact;
		auto duplicateStageArtifact = artifact;
		duplicateStageArtifact.Stages[1].Stage = HE::Rendering::ShaderStage::Vertex;
		Require(HE::EncodeShaderArtifactV2(duplicateStageArtifact, rejectedArtifact).Failed(), "Expected duplicate shader stage rejection");
		auto emptyGlslArtifact = artifact;
		emptyGlslArtifact.Stages[0].GeneratedOpenGlGlsl.clear();
		Require(HE::EncodeShaderArtifactV2(emptyGlslArtifact, rejectedArtifact).Failed(), "Expected empty generated GLSL rejection");
		auto invalidResourceArtifact = artifact;
		invalidResourceArtifact.Interface.Gpu.Resources.front().Type = static_cast<HE::Rendering::ShaderResourceType>(0xff);
		Require(HE::EncodeShaderArtifactV2(invalidResourceArtifact, rejectedArtifact).Failed(), "Expected invalid shader resource type rejection");
		Require(artifact.Interface.Gpu.Stages[0].Outputs.size() == 1 && artifact.Interface.Gpu.Stages[1].Inputs.size() == 1, "Expected matching reflected stage interface");
		Require(artifact.OpenGlCombinedSamplers.size() == 1 && artifact.Stages[1].GeneratedOpenGlGlsl.find(artifact.OpenGlCombinedSamplers[0].UniformName) != std::string::npos, "Expected stable combined sampler mapping in generated GLSL");
		Require(artifact.OpenGlCombinedSamplers[0].TextureName == "u_Texture", "Expected combined sampler texture to retain the reflected resource name: " + artifact.OpenGlCombinedSamplers[0].TextureName);
		Require(artifact.OpenGlCombinedSamplers[0].SamplerName == "u_TextureSampler", "Expected combined sampler sampler to retain the reflected resource name: " + artifact.OpenGlCombinedSamplers[0].SamplerName);
		Require(artifact.Stages[0].GeneratedOpenGlGlsl.find("varying_location_0") != std::string::npos, "Expected vertex varying name derived from location");
		Require(artifact.Stages[1].GeneratedOpenGlGlsl.find("varying_location_0") != std::string::npos, "Expected fragment varying name derived from location");
		Require(artifact.Interface.Gpu.ConstantBuffers.size() == 3, "Expected reflected constant buffers");
		const auto materialBuffer = std::find_if(artifact.Interface.Gpu.ConstantBuffers.begin(), artifact.Interface.Gpu.ConstantBuffers.end(), [](const auto& value) { return value.Set == 1; });
		Require(materialBuffer != artifact.Interface.Gpu.ConstantBuffers.end() && materialBuffer->Size == 48, "Expected golden material buffer size");
		Require(materialBuffer->Members.size() == 6 && materialBuffer->Members[2].Offset == 8 && materialBuffer->Members[3].Offset == 16 && materialBuffer->Members[4].Offset == 28, "Expected golden constant member offsets");

		WriteText(shaderRoot / "Common.hlsli", "float4 MakePosition(float3 value) { return float4(value * 2.0, 1.0); }\n");
		HE::AssetImportFingerprintInput secondInputs;
		Require(importer.BuildFingerprintInput(importContext, rootHash, secondInputs).Succeeded(), "Expected changed include inputs");
		std::string secondFingerprint;
		Require(HE::ComputeAssetImportFingerprint(secondInputs, secondFingerprint).Succeeded() && firstFingerprint != secondFingerprint, "Expected transitive include to invalidate shader fingerprint");

		auto truncated = importResult.Artifact;
		truncated.Payload.pop_back();
		Require(HE::DecodeShaderArtifactV2(truncated, artifact).Failed(), "Expected truncated Shader Artifact V2 rejection");
		auto wrongVersion = importResult.Artifact;
		wrongVersion.ArtifactVersion = 99;
		Require(HE::DecodeShaderArtifactV2(wrongVersion, artifact).Failed(), "Expected Shader Artifact V2 version rejection");

		const auto invalidPath = shaderRoot / "Invalid.hlsl";
		WriteText(invalidPath, "float4 PSMain() : SV_Target0 { syntax error; }\n");
		HE::Rendering::DxcCompileOutput invalidOutput;
		const auto invalidResult = HE::Rendering::DxcShaderCompiler().Compile({ invalidPath, "PSMain", "ps_6_0", HE::Rendering::ShaderStage::Fragment, { shaderRoot } }, invalidOutput);
		Require(invalidResult.Failed() && invalidResult.Summary.find("fragment stage") != std::string::npos && invalidResult.Summary.find("Invalid.hlsl:") != std::string::npos, "Expected stage and source location in DXC syntax diagnostic");

		WriteText(shaderRoot / "Conflict.hlsl",
			"[[vk::binding(0, 1)]] cbuffer MaterialData : register(b0, space1) { float4 u_Color; };\n"
			"[[vk::binding(0, 1)]] Texture2D u_Texture : register(t0, space1);\n"
			"struct VSOutput { float4 Position : SV_Position; }; VSOutput VSMain(float3 position : POSITION) { VSOutput output; output.Position = float4(position, 1); return output; }\n"
			"float4 PSMain(VSOutput input) : SV_Target0 { return u_Color + u_Texture.Load(int3(0, 0, 0)); }\n");
		const auto conflictDescriptor = shaderRoot / "Conflict.shader";
		WriteText(conflictDescriptor,
			"name: Conflict\nlanguage: HLSL\nsource: Conflict.hlsl\n"
			"stages:\n  vertex: { entry: VSMain, profile: vs_6_0 }\n  fragment: { entry: PSMain, profile: ps_6_0 }\nparameters: {}\n");
		const HE::AssetManifestRecord conflictRecord{ .Guid = "conflict-guid", .AssetId = "Shaders/Conflict.shader", .Kind = HE::AssetKind::Shader, .Source = HE::AssetSource::File, .RelativePath = "Shaders/Conflict.shader", .ImportState = HE::AssetImportState::Registered };
		Require(!importer.Import({ context, conflictRecord, conflictDescriptor, nullptr }).Success, "Expected conflicting SPIR-V binding rejection during import");

		WriteText(shaderRoot / "Unsupported.hlsl",
			"struct VSOutput { float4 Position : SV_Position; };\n"
			"VSOutput VSMain(int2 position : POSITION) { VSOutput output; output.Position = float4(float2(position), 0, 1); return output; }\n"
			"float4 PSMain() : SV_Target0 { return float4(1, 1, 1, 1); }\n");
		const auto unsupportedDescriptor = shaderRoot / "Unsupported.shader";
		WriteText(unsupportedDescriptor,
			"name: Unsupported\nlanguage: HLSL\nsource: Unsupported.hlsl\n"
			"stages:\n  vertex: { entry: VSMain, profile: vs_6_0 }\n  fragment: { entry: PSMain, profile: ps_6_0 }\nparameters: {}\n");
		const HE::AssetManifestRecord unsupportedRecord{ .Guid = "unsupported-guid", .AssetId = "Shaders/Unsupported.shader", .Kind = HE::AssetKind::Shader, .Source = HE::AssetSource::File, .RelativePath = "Shaders/Unsupported.shader", .ImportState = HE::AssetImportState::Registered };
		const auto unsupportedResult = importer.Import({ context, unsupportedRecord, unsupportedDescriptor, nullptr });
		Require(!unsupportedResult.Success, "Expected integer vector shader interface rejection during import");
	}
}

int main() {
	const auto root = std::filesystem::temp_directory_path() / "HuaEngineShaderInterfaceSmoke";
	std::error_code errorCode;
	std::filesystem::remove_all(root, errorCode);
	std::filesystem::create_directories(root, errorCode);
	Require(!errorCode, "Expected smoke directory creation");
	TestDescriptorRoundTrip(root);
	TestCanonicalIdentity();
	TestImportFingerprint();
	TestHlslImport(root / "HlslProject");
	std::filesystem::remove_all(root, errorCode);
	Require(!errorCode, "Expected smoke directory cleanup");
	std::cout << "ShaderInterfaceSmoke passed" << std::endl;
	return 0;
}
