#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "HuaEngine.h"
#include "HuaEngine/Asset/Artifact/TextureArtifact.h"
#include "HuaEngine/Asset/Artifact/ShaderArtifact.h"
#include "HuaEngine/Asset/Import/HlslShaderImporter.h"
#include "HuaEngine/Asset/AssetResolver.h"
#include "HuaEngine/Asset/AssetService.h"
#include "HuaEngine/Project/ProjectService.h"
#include "HuaEngine/Rendering/RenderGraph/RenderGraphBuilder.h"
#include "HuaEngine/Rendering/RenderPipeline/RenderBindGroupBuilder.h"
#include "HuaEngine/Rendering/RenderPipeline/UniformBufferArena.h"
#include "HuaEngine/Rendering/RHI/CommandList.h"
#include "HuaEngine/Rendering/RHI/ResourceBarrier.h"
#include "HuaEngine/Rendering/RHI/ResourceStateTracker.h"
#include "HuaEngine/Rendering/RHI/RenderHardwareInterface.h"
#include "HuaEngine/Rendering/RHI/ShaderProgramLoader.h"
#include "Support/TestTextureFixture.h"

namespace {
	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[RHIResourceCreationSmoke] " << message << std::endl;
			std::exit(1);
		}
	}

	HE::ApplicationSpecification MakeApplicationSpecification() {
		HE::ApplicationSpecification specification;
		specification.Name = "RHIResourceCreationSmoke";
		specification.EnableGuiLayer = false;
		specification.EnableWindow = true;
		return specification;
	}

	class SmokeApplication final : public HE::Application {
	public:
		SmokeApplication()
			: HE::Application(MakeApplicationSpecification()) {}
	};

	class BarrierCaptureCommandList final : public HE::Rendering::CommandList {
	public:
		std::vector<HE::Rendering::ResourceBarrier> Barriers;
		std::vector<HE::Rendering::RenderPassDesc> RenderPasses;
		uint32_t EndRenderPassCount = 0;

		void BeginRenderPass(const HE::Rendering::RenderPassDesc& desc) override { RenderPasses.push_back(desc); }
		void EndRenderPass() override { ++EndRenderPassCount; }
		void ResourceBarrier(const HE::Rendering::ResourceBarrier& barrier) override { Barriers.push_back(barrier); }
		void BeginRenderTarget(HE::Rendering::RenderTarget&) override {}
		void ClearColor(const glm::vec4&) override {}
		void BeginFrame() override {}
		void SetPipelineState(HE::Rendering::PipelineState&) override {}
		void SetVertexBuffer(uint32_t, const HE::Rendering::VertexBufferBinding&) override {}
		void SetIndexBuffer(const HE::Rendering::IndexBufferBinding&) override {}
		void SetVertexBufferView(HE::Rendering::VertexBufferView&) override {}
		void SetBindGroup(uint32_t, HE::Rendering::BindGroup&) override {}
		void DrawIndexed(uint32_t) override {}
		void EndFrame() override {}
		void EndRenderTarget() override {}
	};
}

int main() {
	HE::Log::Init({ .EnableConsoleOutput = false });
	const auto smokeRoot = std::filesystem::temp_directory_path() / "HuaEngineRHIResourceCreationSmoke";
	std::error_code smokeError;
	std::filesystem::remove_all(smokeRoot, smokeError);
	Require(!smokeError, "Expected smoke directory cleanup before test");
	const auto texturePath = smokeRoot / "Fixtures" / "Tiny.png";
	Require(HE::Tests::WriteTinyPng(texturePath), "Expected texture fixture creation to succeed");

	SmokeApplication application;
	application.Start();

	auto& device = HE::Rendering::RenderHardwareInterface::GetDevice();
	Require(device.GetDesc().Backend == HE::Rendering::RenderBackendType::OpenGL, "Expected default render backend to be OpenGL");
	Require(device.GetCapabilities().Backend == HE::Rendering::RenderBackendType::OpenGL, "Expected OpenGL device capabilities");
	Require(device.GetCapabilities().SupportsPipelineState, "Expected pipeline state support capability");
	Require(device.GetCapabilities().SupportsBindGroups, "Expected bind group support capability");
	Require(device.GetCapabilities().SupportsCommandSubmission, "Expected command submission support capability");
	Require(!HE::Rendering::RenderHardwareInterface::CreateRenderDevice({ .Backend = HE::Rendering::RenderBackendType::Null }), "Expected unimplemented null backend creation to fail");

	const auto shaderRoot = smokeRoot / "ShaderProject" / "Assets" / "Shaders";
	std::filesystem::create_directories(shaderRoot);
	{
		std::ofstream hlsl(shaderRoot / "Runtime.hlsl");
		hlsl << "struct VSOutput { float4 Position : SV_Position; }; VSOutput VSMain(float3 position : POSITION) { VSOutput output; output.Position = float4(position, 1); return output; }\n"
			"float4 PSMain(VSOutput input) : SV_Target0 { return float4(0.25, 0.5, 0.75, 1.0); }\n";
		std::ofstream descriptor(shaderRoot / "Runtime.shader");
		descriptor << "name: Runtime\nlanguage: HLSL\nsource: Runtime.hlsl\nstages:\n  vertex: { entry: VSMain, profile: vs_6_0 }\n  fragment: { entry: PSMain, profile: ps_6_0 }\nparameters: {}\n";
	}
	HE::ProjectContext shaderContext;
	shaderContext.RootPath = smokeRoot / "ShaderProject";
	shaderContext.ProjectFilePath = shaderContext.RootPath / ".huaengine" / "project.json";
	const HE::AssetManifestRecord shaderRecord{ .Guid = "runtime-shader-guid", .AssetId = "Shaders/Runtime.shader", .Kind = HE::AssetKind::Shader, .Source = HE::AssetSource::File, .RelativePath = "Shaders/Runtime.shader", .ImportState = HE::AssetImportState::Registered };
	const auto shaderImport = HE::HlslShaderImporter().Import({ shaderContext, shaderRecord, shaderRoot / "Runtime.shader", nullptr });
	Require(shaderImport.Success, shaderImport.Diagnostics.empty() ? "Expected HLSL shader import" : shaderImport.Diagnostics.front().Message);
	std::filesystem::remove(shaderRoot / "Runtime.hlsl");
	std::filesystem::remove(shaderRoot / "Runtime.shader");
	HE::ShaderArtifactDataV2 shaderArtifact;
	Require(HE::DecodeShaderArtifactV2(shaderImport.Artifact, shaderArtifact).Succeeded(), "Expected runtime Shader Artifact V2 decode after source removal");
	std::string generatedVertex;
	std::string generatedFragment;
	for (const auto& stage : shaderArtifact.Stages) {
		if (stage.Stage == HE::Rendering::ShaderStage::Vertex) generatedVertex = stage.GeneratedOpenGlGlsl;
		else generatedFragment = stage.GeneratedOpenGlGlsl;
	}
	Require(static_cast<bool>(HE::Rendering::ShaderProgramLoader::CreateFromSource(generatedVertex, generatedFragment)), "Expected OpenGL program creation from generated Artifact GLSL");

	auto schemaMaterial = HE::Rendering::Material::Create("SchemaMaterial", HE::Rendering::MaterialType::Custom);
	Require(static_cast<bool>(schemaMaterial), "Expected schema material creation to succeed");
	schemaMaterial->AddParameter({ "u_Roughness", HE::Rendering::MaterialParameterType::Float, 0.5f });
	schemaMaterial->AddParameter({ "u_BaseColor", HE::Rendering::MaterialParameterType::Vec4, glm::vec4(1.0f) });
	auto schemaInstance = schemaMaterial->CreateInstance();
	Require(static_cast<bool>(schemaInstance), "Expected schema material instance creation to succeed");
	schemaInstance->SetParameter("u_BaseColor", glm::vec4(0.25f, 0.5f, 0.75f, 1.0f));
	Require(schemaInstance->HasParameterOverride("u_BaseColor"), "Expected material instance override");

	float vertices[] = {
		-0.5f, -0.5f, 0.0f,
		 0.5f, -0.5f, 0.0f,
		 0.0f,  0.5f, 0.0f
	};
	HE::Rendering::GpuBufferDesc vertexDesc;
	vertexDesc.Usage = HE::Rendering::GpuBufferUsage::Vertex;
	vertexDesc.Size = sizeof(vertices);
	vertexDesc.Stride = 3 * sizeof(float);
	auto vertexBuffer = device.CreateBuffer(vertexDesc, vertices);
	Require(static_cast<bool>(vertexBuffer), "Expected vertex GPU buffer creation to succeed");
	Require(vertexBuffer->GetDesc().Usage == HE::Rendering::GpuBufferUsage::Vertex, "Expected vertex GPU buffer usage");
	Require(vertexBuffer->GetDesc().Size == sizeof(vertices), "Expected vertex GPU buffer size");

	uint32_t indices[] = { 0, 1, 2 };
	HE::Rendering::GpuBufferDesc indexDesc;
	indexDesc.Usage = HE::Rendering::GpuBufferUsage::Index;
	indexDesc.Size = sizeof(indices);
	indexDesc.Stride = sizeof(uint32_t);
	auto indexBuffer = device.CreateBuffer(indexDesc, indices);
	Require(static_cast<bool>(indexBuffer), "Expected index GPU buffer creation to succeed");
	Require(indexBuffer->GetDesc().Usage == HE::Rendering::GpuBufferUsage::Index, "Expected index GPU buffer usage");

	HE::Rendering::BufferLayout layout = {
		{ HE::Rendering::ShaderDataType::Float3, "a_Position" }
	};
	HE::Rendering::VertexBufferViewDesc viewDesc;
	viewDesc.VertexBuffer = vertexBuffer;
	viewDesc.IndexBuffer = indexBuffer;
	viewDesc.Layout = layout;
	viewDesc.IndexCount = 3;
	auto vertexBufferView = device.CreateVertexBufferView(viewDesc);
	Require(static_cast<bool>(vertexBufferView), "Expected vertex buffer view creation to succeed");
	Require(vertexBufferView->GetDesc().IndexCount == 3, "Expected vertex buffer view index count");

	HE::Rendering::VertexBufferBinding vertexBinding{
		.Buffer = vertexBuffer,
		.Offset = 0,
		.Stride = 3 * sizeof(float)
	};
	Require(vertexBinding.Buffer == vertexBuffer, "Expected vertex binding buffer to round-trip");
	Require(vertexBinding.Stride == 3 * sizeof(float), "Expected vertex binding stride");

	HE::Rendering::IndexBufferBinding indexBinding{
		.Buffer = indexBuffer,
		.Offset = 0,
		.Format = HE::Rendering::IndexFormat::UInt32,
		.IndexCount = 3
	};
	Require(indexBinding.Buffer == indexBuffer, "Expected index binding buffer to round-trip");
	Require(indexBinding.IndexCount == 3, "Expected index binding count");

	struct MatrixIntegerVertex {
		float Transform[16];
		int Ids[2];
	};
	MatrixIntegerVertex matrixIntegerVertices[] = {
		{ { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, -0.5f, -0.5f, 0.0f, 1.0f }, { 0, 10 } },
		{ { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,  0.5f, -0.5f, 0.0f, 1.0f }, { 1, 11 } },
		{ { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,  0.0f,  0.5f, 0.0f, 1.0f }, { 2, 12 } }
	};
	HE::Rendering::GpuBufferDesc matrixIntegerVertexDesc;
	matrixIntegerVertexDesc.Usage = HE::Rendering::GpuBufferUsage::Vertex;
	matrixIntegerVertexDesc.Size = sizeof(matrixIntegerVertices);
	matrixIntegerVertexDesc.Stride = sizeof(MatrixIntegerVertex);
	auto matrixIntegerVertexBuffer = device.CreateBuffer(matrixIntegerVertexDesc, matrixIntegerVertices);
	Require(static_cast<bool>(matrixIntegerVertexBuffer), "Expected matrix/integer vertex GPU buffer creation to succeed");

	HE::Rendering::BufferLayout matrixIntegerLayout = {
		{ HE::Rendering::ShaderDataType::Mat4, "a_Transform" },
		{ HE::Rendering::ShaderDataType::Int2, "a_Ids" }
	};
	HE::Rendering::VertexBufferViewDesc matrixIntegerViewDesc;
	matrixIntegerViewDesc.VertexBuffer = matrixIntegerVertexBuffer;
	matrixIntegerViewDesc.IndexBuffer = indexBuffer;
	matrixIntegerViewDesc.Layout = matrixIntegerLayout;
	matrixIntegerViewDesc.IndexCount = 3;
	auto matrixIntegerVertexBufferView = device.CreateVertexBufferView(matrixIntegerViewDesc);
	Require(static_cast<bool>(matrixIntegerVertexBufferView), "Expected matrix/integer vertex buffer view creation to succeed");

	HE::Rendering::RenderTargetSpecification frameBufferSpec;
	frameBufferSpec.Width = 64;
	frameBufferSpec.Height = 64;
	frameBufferSpec.Attachments = { HE::Rendering::RenderTargetTextureFormat::RGBA8, HE::Rendering::RenderTargetTextureFormat::DEPTH24_STENCIL8 };
	auto renderTarget = device.CreateRenderTarget({ .Specification = frameBufferSpec });
	Require(static_cast<bool>(renderTarget), "Expected render target creation to succeed");
	Require(renderTarget->GetSpecification().Width == 64, "Expected render target width");
	Require(renderTarget->GetSpecification().Height == 64, "Expected render target height");
	const auto colorAttachmentView = renderTarget->GetColorAttachmentView(0);
	Require(colorAttachmentView.NativeHandle != 0, "Expected color attachment native handle");
	Require(colorAttachmentView.Format == HE::Rendering::RenderTargetTextureFormat::RGBA8, "Expected color attachment format metadata");
	Require(colorAttachmentView.Width == 64 && colorAttachmentView.Height == 64, "Expected color attachment size metadata");
	Require(colorAttachmentView.Samples == 1, "Expected color attachment sample metadata");
	Require(colorAttachmentView.AttachmentIndex == 0, "Expected color attachment index metadata");
	const auto depthAttachmentView = renderTarget->GetDepthStencilAttachmentView();
	Require(depthAttachmentView.NativeHandle != 0, "Expected depth/stencil attachment native handle");
	Require(depthAttachmentView.Format == HE::Rendering::RenderTargetTextureFormat::DEPTH24_STENCIL8, "Expected depth/stencil attachment format metadata");
	Require(depthAttachmentView.Width == 64 && depthAttachmentView.Height == 64, "Expected depth/stencil attachment size metadata");
	Require(depthAttachmentView.Samples == 1, "Expected depth/stencil attachment sample metadata");
	const auto colorAttachmentTexture = renderTarget->GetColorAttachmentTexture(0);
	Require(static_cast<bool>(colorAttachmentTexture), "Expected color attachment texture resource");
	Require(colorAttachmentTexture->GetDesc().Format == HE::Rendering::RenderTargetTextureFormat::RGBA8, "Expected color attachment texture format");
	Require((colorAttachmentTexture->GetDesc().Usage & HE::Rendering::TextureUsageColorAttachment) != 0, "Expected color attachment usage");
	Require((colorAttachmentTexture->GetDesc().Usage & HE::Rendering::TextureUsageSampled) != 0, "Expected color attachment sampled usage");
	const auto depthAttachmentTexture = renderTarget->GetDepthStencilAttachmentTexture();
	Require(static_cast<bool>(depthAttachmentTexture), "Expected depth/stencil attachment texture resource");
	Require(depthAttachmentTexture->GetDesc().Format == HE::Rendering::RenderTargetTextureFormat::DEPTH24_STENCIL8, "Expected depth/stencil attachment texture format");
	Require((depthAttachmentTexture->GetDesc().Usage & HE::Rendering::TextureUsageDepthStencilAttachment) != 0, "Expected depth/stencil attachment usage");
	const auto colorAttachmentTextureView = renderTarget->GetColorAttachmentTextureView(0);
	Require(static_cast<bool>(colorAttachmentTextureView), "Expected color attachment texture view");
	Require(colorAttachmentTextureView->GetDesc().Texture == colorAttachmentTexture, "Expected color attachment texture view source");
	const auto depthAttachmentTextureView = renderTarget->GetDepthStencilAttachmentTextureView();
	Require(static_cast<bool>(depthAttachmentTextureView), "Expected depth/stencil attachment texture view");
	Require(depthAttachmentTextureView->GetDesc().Texture == depthAttachmentTexture, "Expected depth/stencil attachment texture view source");

	auto texture = device.CreateTexture({ .SourcePath = texturePath.generic_string() });
	Require(static_cast<bool>(texture), "Expected texture resource creation to succeed");
	Require(texture->GetWidth() > 0 && texture->GetHeight() > 0, "Expected texture dimensions");
	Require(texture->GetDesc().Width == texture->GetWidth(), "Expected file-backed texture desc width to round-trip");
	Require(texture->GetDesc().Height == texture->GetHeight(), "Expected file-backed texture desc height to round-trip");
	Require(texture->GetDesc().Format == HE::Rendering::RenderTargetTextureFormat::RGBA8, "Expected file-backed texture format");
	Require((texture->GetDesc().Usage & HE::Rendering::TextureUsageSampled) != 0, "Expected file-backed texture sampled usage");

	auto emptyTexture = device.CreateTexture({
		.Width = 32,
		.Height = 16,
		.Format = HE::Rendering::RenderTargetTextureFormat::RGBA8,
		.Usage = HE::Rendering::TextureUsageSampled | HE::Rendering::TextureUsageCopyDst,
		.MipLevels = 1,
		.Samples = 1
	});
	Require(static_cast<bool>(emptyTexture), "Expected non-file-backed texture creation to succeed");
	Require(emptyTexture->GetWidth() == 32, "Expected non-file-backed texture width");
	Require(emptyTexture->GetHeight() == 16, "Expected non-file-backed texture height");
	Require(emptyTexture->GetDesc().Format == HE::Rendering::RenderTargetTextureFormat::RGBA8, "Expected non-file-backed texture format");
	Require((emptyTexture->GetDesc().Usage & HE::Rendering::TextureUsageCopyDst) != 0, "Expected non-file-backed texture usage");
	Require(emptyTexture->GetDesc().MipLevels == 1, "Expected non-file-backed texture mip levels");
	Require(emptyTexture->GetDesc().Samples == 1, "Expected non-file-backed texture sample count");
	std::vector<uint8_t> textureUploadData(32 * 16 * 4, 0);
	textureUploadData[0] = 32;
	textureUploadData[1] = 64;
	textureUploadData[2] = 128;
	textureUploadData[3] = 255;
	Require(device.UploadTexture({ .Texture = emptyTexture, .Data = textureUploadData }), "Expected texture upload to succeed");
	std::vector<uint8_t> textureReadbackData;
	Require(device.ReadbackTexture(emptyTexture, 0, textureReadbackData), "Expected texture readback to succeed");
	Require(textureReadbackData == textureUploadData, "Expected texture readback data to match upload");
	Require(!device.UploadTexture({ .Texture = emptyTexture, .Data = { 1, 2, 3 } }), "Expected invalid texture upload size to fail");

	const auto textureProjectRoot = smokeRoot / "TextureResolverProject";
	std::error_code textureError;
	HE::ProjectService projectService;
	HE::ProjectContext textureProject;
	Require(projectService.InitializeProject(textureProjectRoot, &textureProject, "TextureResolverProject").Succeeded(), "Expected texture project initialization");
	const auto importedTexturePath = textureProject.GetAssetRootPath() / "Textures" / "Imported.png";
	std::filesystem::create_directories(importedTexturePath.parent_path());
	std::filesystem::copy_file(texturePath, importedTexturePath, std::filesystem::copy_options::overwrite_existing);

	HE::AssetService textureAssetService;
	HE::AssetHandle importedTextureHandle = 0;
	Require(
		textureAssetService.RegisterTextureAsset(textureProject, "Textures/Imported.png", nullptr, &importedTextureHandle).Succeeded(),
		"Expected texture source registration");
	HE::AssetImportReport textureImportReport;
	Require(textureAssetService.InitializeProjectAssets(textureProject, &textureImportReport).Succeeded(), "Expected texture asset initialization");
	Require(textureImportReport.ImportedAssets == 8 && textureImportReport.FailedAssets == 0, "Expected PNG artifact and seven builtin asset imports");

	HE::AssetRecord importedTextureRecord;
	Require(textureAssetService.ResolveAsset(importedTextureHandle, importedTextureRecord).Succeeded(), "Expected imported texture record");
	HE::AssetArtifact importedTextureArtifact;
	Require(textureAssetService.GetLibrary().ReadArtifact(importedTextureRecord.Guid, importedTextureArtifact).Succeeded(), "Expected imported texture artifact");
	HE::TextureArtifactData importedTextureData;
	Require(HE::DecodeTextureArtifact(importedTextureArtifact, importedTextureData).Succeeded(), "Expected imported texture artifact decode");
	std::filesystem::remove(importedTexturePath, textureError);
	Require(!textureError, "Expected imported texture source removal");

	HE::AssetResolver textureResolver(textureAssetService);
	HE::Ref<HE::Rendering::TextureResource> resolvedTexture;
	Require(textureResolver.ResolveTexture(importedTextureRecord.Guid, resolvedTexture).Succeeded(), "Expected texture resolve from Library");
	Require(resolvedTexture && resolvedTexture->GetWidth() == importedTextureData.Width, "Expected resolved texture dimensions");
	Require(resolvedTexture->GetDesc().SourcePath.empty(), "Expected resolved texture not to retain a source file path");
	std::vector<uint8_t> resolvedTexturePixels;
	Require(device.ReadbackTexture(resolvedTexture, 0, resolvedTexturePixels), "Expected resolved texture readback");
	Require(resolvedTexturePixels == importedTextureData.Pixels, "Expected resolved texture upload to match artifact pixels");
	HE::Ref<HE::Rendering::TextureResource> cachedResolvedTexture;
	Require(textureResolver.ResolveTexture(importedTextureRecord.Guid, cachedResolvedTexture).Succeeded(), "Expected cached texture resolve");
	Require(cachedResolvedTexture == resolvedTexture, "Expected texture runtime cache identity");

	const auto importedShaderPath = textureProject.GetAssetRootPath() / "Shaders" / "Imported.shader";
	const auto importedHlslPath = textureProject.GetAssetRootPath() / "Shaders" / "Imported.hlsl";
	std::filesystem::create_directories(importedShaderPath.parent_path());
	std::ofstream shaderStream(importedShaderPath, std::ios::out | std::ios::binary | std::ios::trunc);
	Require(shaderStream.good(), "Expected project shader source open");
	shaderStream << "name: Imported\nlanguage: HLSL\nsource: Imported.hlsl\nstages:\n  vertex: { entry: VSMain, profile: vs_6_0 }\n  fragment: { entry: PSMain, profile: ps_6_0 }\nparameters:\n  u_Texture:\n    scope: Material\n    editor: Texture2D\n    default: ''\n";
	shaderStream.close();
	Require(shaderStream.good(), "Expected project shader source write");
	std::ofstream hlslStream(importedHlslPath, std::ios::out | std::ios::binary | std::ios::trunc);
	hlslStream << "[[vk::binding(1,1)]] Texture2D u_Texture : register(t1, space1); [[vk::binding(2,1)]] SamplerState u_TextureSampler : register(s2, space1); struct V { float4 Position : SV_Position; float2 Uv : TEXCOORD0; }; V VSMain(float3 p : POSITION, float2 uv : TEXCOORD0) { V o; o.Position=float4(p,1); o.Uv=uv; return o; } float4 PSMain(V i) : SV_Target0 { return u_Texture.Sample(u_TextureSampler, i.Uv); }";
	hlslStream.close();
	HE::AssetHandle importedShaderHandle = 0;
	Require(
		textureAssetService.RegisterShaderAsset(textureProject, "Shaders/Imported.shader", &importedShaderHandle).Succeeded(),
		"Expected project shader source registration");
	HE::AssetImportReport shaderImportReport;
	Require(textureAssetService.InitializeProjectAssets(textureProject, &shaderImportReport).Succeeded(), "Expected project shader artifact import");
	Require(shaderImportReport.ImportedAssets == 1 && shaderImportReport.FailedAssets == 0, "Expected one project shader artifact import");
	HE::AssetRecord importedShaderRecord;
	Require(textureAssetService.ResolveAsset(importedShaderHandle, importedShaderRecord).Succeeded(), "Expected imported shader record");
	Require(importedShaderRecord.Kind == HE::AssetKind::Shader, "Expected imported shader asset kind");

	const auto importedMaterialPath = textureProject.GetAssetRootPath() / "Materials" / "ImportedTextured.material";
	std::filesystem::create_directories(importedMaterialPath.parent_path());
	std::ofstream materialStream(importedMaterialPath, std::ios::out | std::ios::binary | std::ios::trunc);
	Require(materialStream.good(), "Expected textured material source open");
	materialStream <<
		"name: ImportedTexturedMaterial\n"
		"material_type: Unlit\n"
		"shader_guid: " << importedShaderRecord.Guid << "\n"
		"parameters:\n"
		"  u_Texture: Textures/Imported.png\n";
	materialStream.close();
	Require(materialStream.good(), "Expected textured material source write");

	HE::AssetHandle importedMaterialHandle = 0;
	Require(
		textureAssetService.LoadMaterialAsset(textureProject, "Materials/ImportedTextured.material", &importedMaterialHandle).Succeeded(),
		"Expected textured material source registration");
	HE::AssetImportReport materialImportReport;
	Require(textureAssetService.InitializeProjectAssets(textureProject, &materialImportReport).Succeeded(), "Expected textured material import");
	Require(materialImportReport.ImportedAssets == 1 && materialImportReport.FailedAssets == 0, "Expected material artifact import beside skipped texture");
	textureAssetService.GetRuntimeCache() = HE::AssetRuntimeCache();
	HE::AssetRecord importedMaterialRecord;
	Require(textureAssetService.ResolveAsset(importedMaterialHandle, importedMaterialRecord).Succeeded(), "Expected imported material record");
	HE::Ref<HE::Rendering::Material> resolvedTexturedMaterial;
	Require(textureResolver.ResolveMaterial(importedMaterialRecord.Guid, resolvedTexturedMaterial).Succeeded(), "Expected textured material resolve from Library");
	Require(resolvedTexturedMaterial->GetShaderProgram() != nullptr, "Expected material shader GUID resolution");
	Require(resolvedTexturedMaterial->GetShaderGuid() == importedShaderRecord.Guid, "Expected material shader GUID metadata");
	const auto* resolvedTextureParameter = resolvedTexturedMaterial->GetParameter("u_Texture");
	Require(resolvedTextureParameter != nullptr, "Expected resolved material texture parameter");
	Require(
		static_cast<bool>(std::get<HE::Ref<HE::Rendering::TextureResource>>(resolvedTextureParameter->Value)),
		"Expected material texture GUID resolved to an RHI texture");
	Require(std::filesystem::remove(importedShaderPath), "Expected project shader source removal after import");
	textureAssetService.GetRuntimeCache().Invalidate(importedShaderRecord.Guid);
	HE::Ref<HE::Rendering::ShaderProgram> resolvedShader;
	Require(textureResolver.ResolveShader(importedShaderRecord.Guid, resolvedShader).Succeeded(), "Expected shader resolve from Library after source removal");
	Require(resolvedShader != nullptr, "Expected runtime shader program from artifact");
	const auto findStageText = [&](HE::Rendering::ShaderStage stage) {
		const auto& stages = resolvedShader->GetDesc().Stages;
		const auto entry = std::find_if(stages.begin(), stages.end(), [&](const auto& candidate) { return candidate.Stage == stage; });
		return entry == stages.end() ? std::string{} : std::string(entry->Code.begin(), entry->Code.end());
	};
	Require(findStageText(HE::Rendering::ShaderStage::Vertex).find("gl_Position") != std::string::npos, "Expected artifact-backed vertex shader source");
	Require(findStageText(HE::Rendering::ShaderStage::Fragment).find("SPIRV_Cross_Combined") != std::string::npos, "Expected artifact-backed fragment shader source");
	std::filesystem::remove_all(smokeRoot, smokeError);
	Require(!smokeError, "Expected smoke directory cleanup after test");

	Require(!device.CreateTexture({}), "Expected empty texture description to fail");
	Require(!device.CreateTexture({
		.Width = 32,
		.Height = 16,
		.Format = HE::Rendering::RenderTargetTextureFormat::None,
		.Usage = HE::Rendering::TextureUsageSampled
	}), "Expected texture without a concrete format to fail");
	Require(!device.CreateTexture({
		.Width = 32,
		.Height = 16,
		.Format = HE::Rendering::RenderTargetTextureFormat::RGBA8,
		.Usage = HE::Rendering::TextureUsageNone
	}), "Expected texture without usage flags to fail");
	auto textureView = device.CreateTextureView({
		.Texture = emptyTexture,
		.Format = HE::Rendering::RenderTargetTextureFormat::RGBA8,
		.BaseMipLevel = 0,
		.MipLevelCount = 1
	});
	auto mipTexture = device.CreateTexture({
		.Width = 16,
		.Height = 16,
		.Format = HE::Rendering::RenderTargetTextureFormat::RGBA8,
		.Usage = HE::Rendering::TextureUsageSampled | HE::Rendering::TextureUsageCopyDst,
		.MipLevels = 2
	});
	Require(static_cast<bool>(mipTexture), "Expected mip texture creation to succeed");
	Require(static_cast<bool>(device.CreateTextureView({ .Texture = mipTexture, .BaseMipLevel = 1, .MipLevelCount = 1, .Aspect = HE::Rendering::TextureAspect::Color })), "Expected mip texture view creation to succeed");
	Require(!device.CreateTextureView({ .Texture = mipTexture, .BaseMipLevel = 1, .MipLevelCount = 2 }), "Expected out-of-range mip view creation to fail");
	Require(!device.CreateTextureView({ .Texture = mipTexture, .Aspect = HE::Rendering::TextureAspect::Depth }), "Expected incompatible texture aspect to fail");
	Require(!device.ResolveTexture({ .Source = mipTexture, .Destination = emptyTexture }), "Expected OpenGL resolve to reject unsupported non-MSAA resolve");
	Require(static_cast<bool>(textureView), "Expected texture view creation to succeed");
	Require(textureView->GetDesc().Texture == emptyTexture, "Expected texture view resource to round-trip");
	Require(textureView->GetDesc().Format == HE::Rendering::RenderTargetTextureFormat::RGBA8, "Expected texture view format to round-trip");
	auto sampler = device.CreateSampler({
		.MinFilter = HE::Rendering::SamplerFilter::Nearest,
		.MagFilter = HE::Rendering::SamplerFilter::Linear,
		.AddressU = HE::Rendering::SamplerAddressMode::ClampToEdge,
		.AddressV = HE::Rendering::SamplerAddressMode::Repeat,
		.AddressW = HE::Rendering::SamplerAddressMode::Repeat
	});
	Require(static_cast<bool>(sampler), "Expected sampler creation to succeed");
	Require(sampler->GetDesc().MinFilter == HE::Rendering::SamplerFilter::Nearest, "Expected sampler min filter to round-trip");
	Require(sampler->GetDesc().AddressU == HE::Rendering::SamplerAddressMode::ClampToEdge, "Expected sampler address mode to round-trip");
	auto sampledTextureLayout = device.CreateBindGroupLayout({
		.Scope = HE::Rendering::BindGroupScope::Material,
		.Entries = {
			{
				.Name = "u_Texture",
				.Type = HE::Rendering::BindingValueType::TextureView,
				.Binding = 0
			},
			{
				.Name = "u_TextureSampler",
				.Type = HE::Rendering::BindingValueType::Sampler,
				.Binding = 1
			}
		}
	});
	Require(static_cast<bool>(sampledTextureLayout), "Expected texture view/sampler layout creation to succeed");
	auto sampledTextureBindGroup = device.CreateBindGroup({
		.Layout = sampledTextureLayout,
		.Entries = {
			{
				.Name = "u_Texture",
				.Type = HE::Rendering::BindingValueType::TextureView,
				.Value = colorAttachmentTextureView,
				.Binding = 0,
				.TextureSlot = 0
			},
			{
				.Name = "u_TextureSampler",
				.Type = HE::Rendering::BindingValueType::Sampler,
				.Value = sampler,
				.Binding = 1,
				.TextureSlot = 0
			}
		}
	});
	Require(static_cast<bool>(sampledTextureBindGroup), "Expected texture view/sampler bind group creation to succeed");
	Require(std::get<HE::Ref<HE::Rendering::TextureView>>(sampledTextureBindGroup->GetDesc().Entries[0].Value) == colorAttachmentTextureView, "Expected bind group to retain render target attachment view");
	renderTarget->Resize(32, 48);
	Require(renderTarget->GetColorAttachmentTexture(0) == colorAttachmentTexture, "Expected resize to preserve color attachment texture identity");
	Require(colorAttachmentTexture->GetWidth() == 32 && colorAttachmentTexture->GetHeight() == 48, "Expected color attachment texture dimensions after resize");
	Require(colorAttachmentTextureView->GetDesc().Texture == colorAttachmentTexture, "Expected color attachment view source after resize");
	const auto resizedColorAttachmentView = renderTarget->GetColorAttachmentView(0);
	Require(resizedColorAttachmentView.Width == 32 && resizedColorAttachmentView.Height == 48, "Expected color attachment metadata after resize");
	Require(!device.CreateTextureView({}), "Expected empty texture view creation to fail");

	HE::Rendering::RenderGraph attachmentSamplingGraph;
	HE::Rendering::RenderGraphBuilder attachmentSamplingBuilder(attachmentSamplingGraph);
	const auto attachmentColorHandle = attachmentSamplingBuilder.ImportTexture("AttachmentColor", colorAttachmentTexture);
	bool writerPassUsedAttachmentView = false;
	bool readerPassUsedAttachmentTexture = false;
	HE::Ref<HE::Rendering::TextureView> sampledAttachmentView;
	attachmentSamplingBuilder.AddPass("WriteAttachment", HE::Rendering::RenderGraphPassType::Graphics, [&](HE::Rendering::RenderGraphPassBuilder& pass) {
		pass.WriteColor(attachmentColorHandle, HE::Rendering::LoadOp::Clear, HE::Rendering::StoreOp::Store, { 0.2f, 0.3f, 0.4f, 1.0f });
		pass.SetExecute([&](HE::Rendering::RenderPassContext& context) {
			const auto* runtimeResource = context.GraphResources->GetRuntimeResource(attachmentColorHandle);
			writerPassUsedAttachmentView = runtimeResource
				&& runtimeResource->Texture == colorAttachmentTexture
				&& context.GraphRenderPass
				&& context.GraphRenderPass->ColorAttachments.size() == 1
				&& context.GraphRenderPass->ColorAttachments[0].View
				&& context.GraphRenderPass->ColorAttachments[0].View->GetDesc().Texture == colorAttachmentTexture;
		});
	});
	attachmentSamplingBuilder.AddPass("SampleAttachment", HE::Rendering::RenderGraphPassType::Graphics, [&](HE::Rendering::RenderGraphPassBuilder& pass) {
		pass.Read(attachmentColorHandle, HE::Rendering::ResourceState::ShaderRead);
		pass.SetExecute([&](HE::Rendering::RenderPassContext& context) {
			const auto handle = context.GraphResources->FindByName("AttachmentColor");
			const auto* runtimeResource = context.GraphResources->GetRuntimeResource(handle);
			if (runtimeResource && runtimeResource->Texture == colorAttachmentTexture) {
				sampledAttachmentView = context.Device->CreateTextureView({ .Texture = runtimeResource->Texture });
				readerPassUsedAttachmentTexture = static_cast<bool>(sampledAttachmentView);
			}
		});
	});
	Require(attachmentSamplingGraph.Compile(), "Expected attachment sampling graph compile to succeed");
	std::vector<HE::Rendering::RenderGraphResourceBarrier> attachmentBarrierSequence;
	attachmentSamplingGraph.SetBarrierExecutor([&](const HE::Rendering::RenderGraphResourceBarrier& barrier, HE::Rendering::RenderPassContext&) {
		attachmentBarrierSequence.push_back(barrier);
	});
	HE::Rendering::ResourceStateTracker attachmentResourceStates;
	HE::Rendering::RenderPassContext attachmentSamplingContext;
	BarrierCaptureCommandList attachmentCommands;
	attachmentSamplingContext.Device = &device;
	attachmentSamplingContext.Commands = &attachmentCommands;
	attachmentSamplingContext.ResourceStates = &attachmentResourceStates;
	Require(attachmentSamplingGraph.Execute(attachmentSamplingContext), "Expected attachment sampling graph execute to succeed");
	Require(attachmentSamplingContext.GraphResources == nullptr, "Expected graph resource context to be restored after execute");
	Require(attachmentSamplingContext.GraphRenderPass == nullptr, "Expected graph render-pass context to be restored after execute");
	Require(writerPassUsedAttachmentView, "Expected writer pass to resolve the imported attachment texture");
	Require(attachmentCommands.RenderPasses.size() == 1, "Expected graph attachment pass to begin one render pass");
	Require(attachmentCommands.EndRenderPassCount == 1, "Expected graph attachment pass to end one render pass");
	Require(attachmentCommands.RenderPasses[0].ColorAttachments[0].Load == HE::Rendering::LoadOp::Clear, "Expected graph render pass color load operation");
	Require(attachmentCommands.RenderPasses[0].ColorAttachments[0].ClearColor == glm::vec4(0.2f, 0.3f, 0.4f, 1.0f), "Expected graph render pass clear color");
	Require(readerPassUsedAttachmentTexture, "Expected reader pass to create a sampled view from the imported attachment texture");
	Require(sampledAttachmentView->GetDesc().Texture == colorAttachmentTexture, "Expected sampled attachment view source texture");
	Require(attachmentBarrierSequence.size() == 2, "Expected attachment graph to emit write and sampled-read barriers");
	Require(attachmentBarrierSequence[0].Before == HE::Rendering::ResourceState::Undefined && attachmentBarrierSequence[0].After == HE::Rendering::ResourceState::RenderTarget, "Expected attachment write barrier state transition");
	Require(attachmentBarrierSequence[1].Before == HE::Rendering::ResourceState::RenderTarget && attachmentBarrierSequence[1].After == HE::Rendering::ResourceState::ShaderRead, "Expected attachment sampled-read barrier state transition");
	Require(attachmentResourceStates.GetState(colorAttachmentTexture) == HE::Rendering::ResourceState::ShaderRead, "Expected attachment texture final shader-read state");

	HE::Rendering::RenderGraph transientPoolGraph;
	HE::Rendering::RenderGraphBuilder transientPoolBuilder(transientPoolGraph);
	const auto firstTransientHandle = transientPoolBuilder.CreateTexture("FirstTransient", { .Width = 16, .Height = 16, .Format = HE::Rendering::RenderTargetTextureFormat::RGBA8 });
	const auto secondTransientHandle = transientPoolBuilder.CreateTexture("SecondTransient", { .Width = 16, .Height = 16, .Format = HE::Rendering::RenderTargetTextureFormat::RGBA8 });
	transientPoolBuilder.AddPass("WriteFirstTransient", HE::Rendering::RenderGraphPassType::Graphics, [firstTransientHandle](HE::Rendering::RenderGraphPassBuilder& pass) {
		pass.WriteColor(firstTransientHandle);
		pass.SetExecute([](HE::Rendering::RenderPassContext&) {});
	});
	transientPoolBuilder.AddPass("WriteSecondTransient", HE::Rendering::RenderGraphPassType::Graphics, [secondTransientHandle](HE::Rendering::RenderGraphPassBuilder& pass) {
		pass.WriteColor(secondTransientHandle);
		pass.SetExecute([](HE::Rendering::RenderPassContext&) {});
	});
	Require(transientPoolGraph.Compile(), "Expected transient pool graph compile to succeed");
	HE::Rendering::RenderPassContext transientPoolContext;
	BarrierCaptureCommandList transientPoolCommands;
	transientPoolContext.Device = &device;
	transientPoolContext.Commands = &transientPoolCommands;
	Require(transientPoolGraph.Execute(transientPoolContext), "Expected transient pool graph first execute to succeed");
	const auto firstTransientTexture = transientPoolGraph.GetResourceAllocator().GetRuntimeResource(firstTransientHandle)->Texture;
	const auto secondTransientTexture = transientPoolGraph.GetResourceAllocator().GetRuntimeResource(secondTransientHandle)->Texture;
	Require(firstTransientTexture == secondTransientTexture, "Expected non-overlapping transient lifetimes to alias one texture");
	transientPoolGraph.ReleaseTransientResources(10);
	transientPoolContext.CompletedGraphicsFenceValue = 9;
	Require(transientPoolGraph.Execute(transientPoolContext), "Expected transient pool graph execute before fence completion to succeed");
	const auto blockedTransientTexture = transientPoolGraph.GetResourceAllocator().GetRuntimeResource(firstTransientHandle)->Texture;
	Require(blockedTransientTexture != firstTransientTexture, "Expected unfinished fence to prevent transient texture reuse");
	transientPoolGraph.ReleaseTransientResources(20);
	transientPoolContext.CompletedGraphicsFenceValue = 10;
	Require(transientPoolGraph.Execute(transientPoolContext), "Expected transient pool graph execute after fence completion to succeed");
	const auto reusedTransientTexture = transientPoolGraph.GetResourceAllocator().GetRuntimeResource(firstTransientHandle)->Texture;
	Require(reusedTransientTexture == firstTransientTexture, "Expected completed fence to allow transient texture reuse");
	transientPoolGraph.ReleaseTransientResources(30);

	HE::Rendering::RenderGraph transientBufferPoolGraph;
	HE::Rendering::RenderGraphBuilder transientBufferPoolBuilder(transientBufferPoolGraph);
	const auto firstTransientBuffer = transientBufferPoolBuilder.CreateBuffer("FirstTransientBuffer", { .Size = 256, .Stride = 16, .Usage = HE::Rendering::GpuBufferUsage::Storage });
	const auto secondTransientBuffer = transientBufferPoolBuilder.CreateBuffer("SecondTransientBuffer", { .Size = 256, .Stride = 16, .Usage = HE::Rendering::GpuBufferUsage::Storage });
	transientBufferPoolBuilder.AddPass("WriteFirstTransientBuffer", HE::Rendering::RenderGraphPassType::Copy, [firstTransientBuffer](HE::Rendering::RenderGraphPassBuilder& pass) {
		pass.Write(firstTransientBuffer, HE::Rendering::ResourceState::CopyDst);
		pass.SetExecute([](HE::Rendering::RenderPassContext&) {});
	});
	transientBufferPoolBuilder.AddPass("WriteSecondTransientBuffer", HE::Rendering::RenderGraphPassType::Copy, [secondTransientBuffer](HE::Rendering::RenderGraphPassBuilder& pass) {
		pass.Write(secondTransientBuffer, HE::Rendering::ResourceState::CopyDst);
		pass.SetExecute([](HE::Rendering::RenderPassContext&) {});
	});
	Require(transientBufferPoolGraph.Compile(), "Expected transient buffer pool graph compile to succeed");
	HE::Rendering::RenderPassContext transientBufferContext;
	transientBufferContext.Device = &device;
	Require(transientBufferPoolGraph.Execute(transientBufferContext), "Expected transient buffer pool first execute to succeed");
	const auto firstBuffer = transientBufferPoolGraph.GetResourceAllocator().GetRuntimeResource(firstTransientBuffer)->Buffer;
	const auto secondBuffer = transientBufferPoolGraph.GetResourceAllocator().GetRuntimeResource(secondTransientBuffer)->Buffer;
	Require(firstBuffer == secondBuffer, "Expected non-overlapping transient buffers to alias");
	transientBufferPoolGraph.ReleaseTransientResources(10);
	transientBufferContext.CompletedGraphicsFenceValue = 9;
	Require(transientBufferPoolGraph.Execute(transientBufferContext), "Expected transient buffer graph execute before fence completion to succeed");
	Require(transientBufferPoolGraph.GetResourceAllocator().GetRuntimeResource(firstTransientBuffer)->Buffer != firstBuffer, "Expected unfinished fence to prevent transient buffer reuse");
	transientBufferPoolGraph.ReleaseTransientResources(20);
	transientBufferContext.CompletedGraphicsFenceValue = 10;
	Require(transientBufferPoolGraph.Execute(transientBufferContext), "Expected transient buffer graph execute after fence completion to succeed");
	Require(transientBufferPoolGraph.GetResourceAllocator().GetRuntimeResource(firstTransientBuffer)->Buffer == firstBuffer, "Expected completed fence to allow transient buffer reuse");
	transientBufferPoolGraph.ReleaseTransientResources(30);

	device.GetImmediateCommandList().ResourceBarrier({
		.Texture = texture,
		.Before = HE::Rendering::ResourceState::Undefined,
		.After = HE::Rendering::ResourceState::ShaderRead
	});

	HE::Rendering::RenderGraph runtimeResourceGraph;
	HE::Rendering::RenderGraphBuilder runtimeResourceBuilder(runtimeResourceGraph);
	const auto importedGraphTexture = runtimeResourceBuilder.ImportTexture("ImportedTexture", emptyTexture);
	const auto transientGraphTexture = runtimeResourceBuilder.CreateTexture("TransientTexture", {
			.Width = 16,
			.Height = 8,
			.Format = HE::Rendering::RenderTargetTextureFormat::RGBA8
	});
	runtimeResourceBuilder.AddPass("RuntimeResourcePass", HE::Rendering::RenderGraphPassType::Graphics, [importedGraphTexture, transientGraphTexture](HE::Rendering::RenderGraphPassBuilder& pass) {
		pass.Read(importedGraphTexture, HE::Rendering::ResourceState::ShaderRead);
		pass.Write(transientGraphTexture, HE::Rendering::ResourceState::RenderTarget);
		pass.SetExecute([](HE::Rendering::RenderPassContext&) {});
	});
	Require(runtimeResourceGraph.Compile(), "Expected runtime resource graph compile to succeed");
	HE::Rendering::RenderPassContext runtimeResourceContext;
	runtimeResourceContext.Device = &device;
	Require(runtimeResourceGraph.Execute(runtimeResourceContext), "Expected runtime resource graph execute to succeed");
	const auto* importedRuntimeResource = runtimeResourceGraph.GetResourceAllocator().GetRuntimeResource(importedGraphTexture);
	Require(importedRuntimeResource && importedRuntimeResource->Texture == emptyTexture, "Expected imported graph texture to preserve runtime texture binding");
	const auto* transientRuntimeResource = runtimeResourceGraph.GetResourceAllocator().GetRuntimeResource(transientGraphTexture);
	Require(transientRuntimeResource && transientRuntimeResource->Texture, "Expected transient graph texture to allocate a runtime texture");
	Require(transientRuntimeResource->Texture->GetDesc().Width == 16, "Expected transient graph texture width");
	Require(transientRuntimeResource->Texture->GetDesc().Height == 8, "Expected transient graph texture height");
	Require(transientRuntimeResource->Texture->GetDesc().Format == HE::Rendering::RenderTargetTextureFormat::RGBA8, "Expected transient graph texture format");

	HE::Rendering::RenderGraph stateTrackedGraph;
	HE::Rendering::RenderGraphBuilder stateTrackedBuilder(stateTrackedGraph);
	const auto trackedImportedTexture = stateTrackedBuilder.ImportTexture("TrackedImportedTexture", emptyTexture);
	stateTrackedBuilder.AddPass("ReadTrackedImportedTexture", HE::Rendering::RenderGraphPassType::Graphics, [trackedImportedTexture](HE::Rendering::RenderGraphPassBuilder& pass) {
		pass.Read(trackedImportedTexture, HE::Rendering::ResourceState::ShaderRead);
		pass.SetExecute([](HE::Rendering::RenderPassContext&) {});
	});
	Require(stateTrackedGraph.Compile(), "Expected state tracked graph compile to succeed");
	BarrierCaptureCommandList barrierCaptureCommands;
	HE::Rendering::ResourceStateTracker resourceStates;
	HE::Rendering::RenderPassContext stateTrackedContext;
	stateTrackedContext.Device = &device;
	stateTrackedContext.Commands = &barrierCaptureCommands;
	stateTrackedContext.ResourceStates = &resourceStates;
	Require(stateTrackedGraph.Execute(stateTrackedContext), "Expected state tracked graph execute to succeed");
	Require(barrierCaptureCommands.Barriers.size() == 1, "Expected first graph execute to emit one resource barrier");
	Require(barrierCaptureCommands.Barriers[0].Texture == emptyTexture, "Expected emitted barrier to reference imported runtime texture");
	Require(barrierCaptureCommands.Barriers[0].Before == HE::Rendering::ResourceState::Undefined, "Expected first barrier before state");
	Require(barrierCaptureCommands.Barriers[0].After == HE::Rendering::ResourceState::ShaderRead, "Expected first barrier after state");
	Require(stateTrackedGraph.Execute(stateTrackedContext), "Expected repeated state tracked graph execute to succeed");
	Require(barrierCaptureCommands.Barriers.size() == 1, "Expected repeated graph execute to avoid duplicate same-state barrier");

	const std::string vertexSource = R"(
		#version 330 core
		layout(location = 0) in vec3 a_Position;
		void main() {
			gl_Position = vec4(a_Position, 1.0);
		}
	)";
	const std::string fragmentSource = R"(
		#version 330 core
		layout(location = 0) out vec4 color;
		void main() {
			color = vec4(1.0);
		}
	)";
	HE::Rendering::ShaderProgramDesc simpleShaderDesc;
	Require(HE::Rendering::BuildOpenGlShaderProgramDesc(vertexSource, fragmentSource, {}, {}, simpleShaderDesc).Succeeded(), "Expected simple shader descriptor");
	auto shaderProgram = device.CreateShaderProgram(simpleShaderDesc);
	Require(static_cast<bool>(shaderProgram), "Expected shader program creation to succeed");

	auto pipelineState = device.CreatePipelineState({
		.Shader = shaderProgram,
		.VertexLayout = layout,
		.Topology = HE::Rendering::PrimitiveTopology::TriangleList
	});
	Require(static_cast<bool>(pipelineState), "Expected pipeline state creation to succeed");
	Require(pipelineState->GetDesc().Shader == shaderProgram, "Expected pipeline state shader");
	Require(pipelineState->GetDesc().Topology == HE::Rendering::PrimitiveTopology::TriangleList, "Expected triangle list pipeline topology");
	Require(pipelineState->GetDesc().ColorTargets.size() == 1, "Expected default pipeline color target contract");
	Require(pipelineState->GetDesc().ColorTargets[0].Format == HE::Rendering::RenderTargetTextureFormat::RGBA8, "Expected default pipeline color target format");
	Require(!pipelineState->GetDesc().ColorTargets[0].BlendEnabled, "Expected default pipeline blend disabled");
	Require(pipelineState->GetDesc().DepthStencil.Format == HE::Rendering::RenderTargetTextureFormat::DEPTH24_STENCIL8, "Expected default pipeline depth/stencil format");
	Require(pipelineState->GetDesc().Raster.Cull == HE::Rendering::CullMode::Back, "Expected default pipeline raster cull mode");
	Require(!device.CreatePipelineState({}), "Expected empty pipeline state creation to fail");

	const std::string contractVertexSource = R"(
		#version 330 core
		layout(location = 0) in vec3 a_Position;
		layout(std140) uniform FrameData { mat4 u_ViewProjection; };
		void main() { gl_Position = u_ViewProjection * vec4(a_Position, 1.0); }
	)";
	HE::Rendering::ShaderInterface contractInterface;
	contractInterface.Gpu.Stages = {
		{ HE::Rendering::ShaderStage::Vertex, "main" },
		{ HE::Rendering::ShaderStage::Fragment, "main" }
	};
	contractInterface.Gpu.Resources = {
		{ "FrameData", HE::Rendering::ShaderResourceType::ConstantBuffer, 0, 0, 1, HE::Rendering::ShaderStageVertex }
	};
	contractInterface.Gpu.ConstantBuffers = {
		{ "FrameData", 0, 0, 64, { { "u_ViewProjection", HE::Rendering::ShaderValueType::Float4x4, 0, 64, 16, 0, true } } }
	};
	Require(HE::Rendering::FinalizeShaderInterface(contractInterface).Succeeded(), "Expected pipeline shader interface finalization");
	HE::Rendering::ShaderResourceMap contractResourceMap;
	contractResourceMap.UniformBlocks = {{
		.Name = "FrameData",
		.Set = 0,
		.Binding = 0,
		.BindingPoint = 0,
		.Size = 64,
		.StageMask = HE::Rendering::ShaderStageVertex,
		.Members = {{ .Name = "u_ViewProjection", .Offset = 0, .Size = 64 }}
	}};
	HE::Rendering::ShaderProgramDesc contractShaderDesc;
	Require(HE::Rendering::BuildOpenGlShaderProgramDesc(contractVertexSource, fragmentSource, contractInterface.Gpu, std::move(contractResourceMap), contractShaderDesc).Succeeded(), "Expected contract shader descriptor");
	auto contractShader = device.CreateShaderProgram(contractShaderDesc);
	Require(static_cast<bool>(contractShader), "Expected contract shader creation");
	auto wrongFrameLayout = device.CreateBindGroupLayout({
		.Scope = HE::Rendering::BindGroupScope::Frame,
		.Entries = { { "FrameData", HE::Rendering::BindingValueType::UniformBuffer, 1, HE::Rendering::ShaderStageVertex, 64 } },
		.InterfaceDigest = contractShaderDesc.Interface.Digest
	});
	Require(static_cast<bool>(wrongFrameLayout), "Expected structurally valid but shader-incompatible layout creation");
	Require(!device.CreatePipelineState({
		.Shader = contractShader,
		.VertexLayout = layout,
		.BindGroupLayouts = { { 0, wrongFrameLayout } }
	}), "Expected pipeline creation to reject a ShaderInterface binding mismatch");
	auto contractFrameLayout = HE::Rendering::CreateUniformBlockBindGroupLayout(
		device,
		HE::Rendering::BindGroupScope::Frame,
		contractShaderDesc.ResourceMap.UniformBlocks.front(),
		contractShaderDesc.Interface.Digest);
	Require(static_cast<bool>(contractFrameLayout), "Expected ShaderInterface-compatible frame layout");

	auto bindGroupLayout = device.CreateBindGroupLayout({
		.Scope = HE::Rendering::BindGroupScope::Material,
		.Entries = {
			{
				.Name = "u_Color",
				.Type = HE::Rendering::BindingValueType::Float4,
				.Binding = 0
			}
		}
	});
	Require(static_cast<bool>(bindGroupLayout), "Expected bind group layout creation to succeed");
	auto bindGroup = device.CreateBindGroup({
		.Layout = bindGroupLayout,
		.Entries = {
			{
				.Name = "u_Color",
				.Type = HE::Rendering::BindingValueType::Float4,
				.Value = glm::vec4(1.0f),
				.Binding = 0
			}
		}
	});
	Require(static_cast<bool>(bindGroup), "Expected bind group creation to succeed");
	Require(bindGroup->GetDesc().Layout == bindGroupLayout, "Expected bind group layout to round-trip");
	HE::Rendering::GpuBufferDesc uniformBufferDesc;
	uniformBufferDesc.Usage = HE::Rendering::GpuBufferUsage::Uniform;
	uniformBufferDesc.Size = 64;
	uniformBufferDesc.Stride = 16;
	auto uniformBuffer = device.CreateBuffer(uniformBufferDesc, nullptr);
	Require(static_cast<bool>(uniformBuffer), "Expected uniform buffer creation to succeed");
	const std::vector<uint8_t> uploadedBufferData{ 1, 2, 3, 4, 5, 6, 7, 8 };
	Require(device.UploadBuffer({ .Buffer = uniformBuffer, .Offset = 16, .Data = uploadedBufferData }), "Expected buffer upload to succeed");
	std::vector<uint8_t> readbackBufferData;
	Require(device.ReadbackBuffer(uniformBuffer, 16, static_cast<uint32_t>(uploadedBufferData.size()), readbackBufferData), "Expected buffer readback to succeed");
	Require(readbackBufferData == uploadedBufferData, "Expected buffer readback data to match upload");
	Require(!device.UploadBuffer({ .Buffer = uniformBuffer, .Offset = 60, .Data = uploadedBufferData }), "Expected out-of-range buffer upload to fail");
	HE::Rendering::UniformBufferArena uniformArena(device, 512, 256);
	HE::Rendering::UniformBufferAllocation firstUniformAllocation;
	HE::Rendering::UniformBufferAllocation secondUniformAllocation;
	const glm::vec4 uniformValue(1.0f);
	Require(uniformArena.Allocate(&uniformValue, sizeof(uniformValue), firstUniformAllocation), "Expected first arena allocation to succeed");
	Require(uniformArena.Allocate(&uniformValue, sizeof(uniformValue), secondUniformAllocation), "Expected aligned arena allocation to succeed");
	Require(firstUniformAllocation.Offset == 0 && secondUniformAllocation.Offset == 256, "Expected uniform arena offsets to respect alignment");
	Require(firstUniformAllocation.Buffer == secondUniformAllocation.Buffer && uniformArena.GetBackingBufferCount() == 1, "Expected draw allocations to share one backing buffer");
	uniformArena.SealFrame(5);
	uniformArena.SealFrame(4);
	uniformArena.BeginFrame(4);
	HE::Rendering::UniformBufferAllocation blockedUniformAllocation;
	Require(!uniformArena.Allocate(&uniformValue, sizeof(uniformValue), blockedUniformAllocation), "Expected repeated or backward fence values not to release in-flight arena storage");
	uniformArena.BeginFrame(5);
	Require(uniformArena.Allocate(&uniformValue, sizeof(uniformValue), blockedUniformAllocation) && blockedUniformAllocation.Offset == 0, "Expected completed arena storage to be reusable");
	uniformArena.SealFrame(5);
	uniformArena.BeginFrame(5);
	Require(uniformArena.Allocate(&uniformValue, sizeof(uniformValue), blockedUniformAllocation) && blockedUniformAllocation.Offset == 0, "Expected repeated completed signal to preserve monotonic arena reuse");
	const HE::Rendering::ShaderUniformBlockBinding projectedFrameBlock{
		.Name = "ProjectedFrame",
		.Set = 0,
		.Binding = 4,
		.BindingPoint = 7,
		.Size = 80,
		.StageMask = HE::Rendering::ShaderStageVertex,
		.Members = {{ .Name = "u_ViewProjection", .Offset = 16, .Size = 64 }}
	};
	const HE::Sha256Digest projectedInterfaceDigest{ 1 };
	auto projectedFrameLayout = HE::Rendering::CreateUniformBlockBindGroupLayout(device, HE::Rendering::BindGroupScope::Frame, projectedFrameBlock, projectedInterfaceDigest);
	Require(projectedFrameLayout && projectedFrameLayout->GetDesc().Entries[0].Binding == 7 && projectedFrameLayout->GetDesc().Entries[0].MinBindingSize == 80, "Expected frame layout to use reflected binding point and size");
	const glm::mat4 projectedViewProjection(2.0f);
	auto projectedFrameGroup = HE::Rendering::CreateFrameBindGroup(device, uniformArena, projectedFrameBlock, projectedFrameLayout, projectedViewProjection);
	Require(projectedFrameGroup && projectedFrameGroup->GetDesc().Entries[0].Size == 80, "Expected frame bind group to use reflected block size");
	const auto projectedBuffer = std::get<HE::Ref<HE::Rendering::GpuBuffer>>(projectedFrameGroup->GetDesc().Entries[0].Value);
	std::vector<uint8_t> projectedBytes;
	Require(device.ReadbackBuffer(projectedBuffer, projectedFrameGroup->GetDesc().Entries[0].Offset, 80, projectedBytes), "Expected projected frame buffer readback");
	Require(std::all_of(projectedBytes.begin(), projectedBytes.begin() + 16, [](uint8_t value) { return value == 0; }), "Expected reflected frame member offset to be preserved");
	Require(std::memcmp(projectedBytes.data() + 16, &projectedViewProjection[0][0], 64) == 0, "Expected frame matrix at reflected member offset");
	const std::vector<HE::Rendering::ShaderTextureBinding> projectedTextures{{ .TextureName = "u_Albedo", .UniformName = "u_AlbedoCombined", .TextureUnit = 5, .StageMask = HE::Rendering::ShaderStageFragment }};
	auto projectedMaterialLayout = HE::Rendering::CreateMaterialBindGroupLayout(device, { .Name = "MaterialData", .Set = 1, .BindingPoint = 3, .Size = 16, .StageMask = HE::Rendering::ShaderStageFragment }, projectedTextures, projectedInterfaceDigest);
	Require(projectedMaterialLayout && projectedMaterialLayout->GetDesc().Entries.size() == 2, "Expected material layout entries from shader interface");
	Require(projectedMaterialLayout->GetDesc().Entries[1].Name == "u_AlbedoCombined" && projectedMaterialLayout->GetDesc().Entries[1].Binding == 5, "Expected combined sampler binding from shader resource map");
	HE::Rendering::BindGroupLayoutDesc uniformLayoutDesc{
		.Scope = HE::Rendering::BindGroupScope::Frame,
		.Entries = {{
			.Name = "FrameData",
			.Type = HE::Rendering::BindingValueType::UniformBuffer,
			.Binding = 3,
			.Visibility = HE::Rendering::ShaderStageVertex | HE::Rendering::ShaderStageFragment,
			.MinBindingSize = 16
		}}
	};
	auto uniformLayout = device.CreateBindGroupLayout(uniformLayoutDesc);
	Require(static_cast<bool>(uniformLayout), "Expected uniform buffer bind group layout creation to succeed");
	Require(HE::Rendering::CalculateBindGroupLayoutSignature(uniformLayoutDesc) == HE::Rendering::CalculateBindGroupLayoutSignature(uniformLayout->GetDesc()), "Expected bind group layout signature to be stable");
	auto uniformBindGroup = device.CreateBindGroup({
		.Layout = uniformLayout,
		.Entries = {{
			.Name = "FrameData",
			.Type = HE::Rendering::BindingValueType::UniformBuffer,
			.Value = uniformBuffer,
			.Binding = 3,
			.Offset = 16,
			.Size = 32
		}}
	});
	Require(static_cast<bool>(uniformBindGroup), "Expected ranged uniform buffer bind group creation to succeed");
	Require(!device.CreateBindGroup({
		.Layout = uniformLayout,
		.Entries = {{ .Name = "FrameData", .Type = HE::Rendering::BindingValueType::UniformBuffer, .Value = uniformBuffer, .Binding = 3, .Offset = 48, .Size = 32 }}
	}), "Expected out-of-range uniform buffer binding to fail");

	auto commandBuffer = device.CreateCommandBuffer({
		.Usage = HE::Rendering::CommandBufferUsage::Graphics,
		.DebugName = "RHIResourceCreationSmoke empty command buffer"
	});
	Require(static_cast<bool>(commandBuffer), "Expected command buffer creation to succeed");
	Require(commandBuffer->GetDesc().Usage == HE::Rendering::CommandBufferUsage::Graphics, "Expected graphics command buffer usage");
	Require(commandBuffer->GetDesc().DebugName == "RHIResourceCreationSmoke empty command buffer", "Expected command buffer debug name");
	device.GetGraphicsQueue().Submit(*commandBuffer);

	Require(!device.CreateCommandBuffer({ .Usage = HE::Rendering::CommandBufferUsage::Invalid }), "Expected invalid command buffer creation to fail");

	auto contractedPipelineState = device.CreatePipelineState({
		.Shader = contractShader,
		.VertexLayout = layout,
		.Topology = HE::Rendering::PrimitiveTopology::TriangleList,
		.BindGroupLayouts = {
			{
				.Slot = 0,
				.Layout = contractFrameLayout
			}
		}
	});
	Require(static_cast<bool>(contractedPipelineState), "Expected contracted pipeline state creation to succeed");
	Require(contractedPipelineState->GetDesc().BindGroupLayouts.size() == 1, "Expected pipeline bind group layout contract");
	Require(contractedPipelineState->GetDesc().BindGroupLayouts[0].Slot == 0, "Expected frame bind group slot contract");
	Require(contractedPipelineState->GetDesc().BindGroupLayouts[0].Layout == contractFrameLayout, "Expected frame bind group layout contract");

	auto renderStatePipeline = device.CreatePipelineState({
		.Shader = shaderProgram,
		.VertexLayout = layout,
		.Topology = HE::Rendering::PrimitiveTopology::TriangleList,
		.ColorTargets = {
			{
				.Format = HE::Rendering::RenderTargetTextureFormat::RGBA8,
				.BlendEnabled = true,
				.WriteMask = HE::Rendering::ColorWriteMaskAll
			}
		},
		.DepthStencil = {
			.Format = HE::Rendering::RenderTargetTextureFormat::DEPTH24_STENCIL8,
			.DepthTestEnabled = true,
			.DepthWriteEnabled = true,
			.DepthCompare = HE::Rendering::CompareOp::LessEqual
		},
		.Raster = {
			.Cull = HE::Rendering::CullMode::None,
			.FrontFaceMode = HE::Rendering::FrontFace::CounterClockwise,
			.Fill = HE::Rendering::FillMode::Solid
		}
	});
	Require(static_cast<bool>(renderStatePipeline), "Expected explicit render state pipeline creation to succeed");
	Require(renderStatePipeline->GetDesc().ColorTargets[0].BlendEnabled, "Expected pipeline blend state to round-trip");
	Require(renderStatePipeline->GetDesc().DepthStencil.DepthCompare == HE::Rendering::CompareOp::LessEqual, "Expected depth compare state to round-trip");
	Require(renderStatePipeline->GetDesc().Raster.Cull == HE::Rendering::CullMode::None, "Expected raster state to round-trip");

	Require(!device.CreatePipelineState({
		.Shader = shaderProgram,
		.VertexLayout = layout,
		.Topology = HE::Rendering::PrimitiveTopology::TriangleList,
		.ColorTargets = {}
	}), "Expected pipeline without color targets to fail");
	Require(!device.CreatePipelineState({
		.Shader = shaderProgram,
		.VertexLayout = layout,
		.Topology = HE::Rendering::PrimitiveTopology::TriangleList,
		.ColorTargets = {
			{
				.Format = HE::Rendering::RenderTargetTextureFormat::None
			}
		}
	}), "Expected pipeline with empty color target format to fail");
	Require(!device.CreatePipelineState({
		.Shader = shaderProgram,
		.VertexLayout = layout,
		.Topology = HE::Rendering::PrimitiveTopology::TriangleList,
		.ColorTargets = {
			{
				.Format = HE::Rendering::RenderTargetTextureFormat::DEPTH24_STENCIL8
			}
		}
	}), "Expected pipeline with depth format as color target to fail");
	Require(!device.CreatePipelineState({
		.Shader = shaderProgram,
		.VertexLayout = layout,
		.Topology = HE::Rendering::PrimitiveTopology::TriangleList,
		.DepthStencil = {
			.Format = HE::Rendering::RenderTargetTextureFormat::RGBA8
		}
	}), "Expected pipeline with color format as depth/stencil target to fail");

	Require(!device.CreatePipelineState({
		.Shader = shaderProgram,
		.VertexLayout = layout,
		.Topology = HE::Rendering::PrimitiveTopology::TriangleList,
		.BindGroupLayouts = {
			{
				.Slot = 1,
				.Layout = bindGroupLayout
			},
			{
				.Slot = 1,
				.Layout = bindGroupLayout
			}
		}
	}), "Expected duplicate pipeline bind group layout slots to fail");
	Require(!device.CreatePipelineState({
		.Shader = shaderProgram,
		.VertexLayout = layout,
		.Topology = HE::Rendering::PrimitiveTopology::TriangleList,
		.BindGroupLayouts = {
			{
				.Slot = 1,
				.Layout = nullptr
			}
		}
	}), "Expected null pipeline bind group layout to fail");

	Require(!device.CreateBindGroupLayout({}), "Expected empty bind group layout creation to fail");
	Require(!device.CreateBindGroup({}), "Expected empty bind group creation to fail");

	HE::Rendering::GpuBufferDesc invalidBufferDesc;
	invalidBufferDesc.Usage = HE::Rendering::GpuBufferUsage::Vertex;
	invalidBufferDesc.Size = 0;
	Require(!device.CreateBuffer(invalidBufferDesc, nullptr), "Expected zero-sized GPU buffer creation to fail");

	std::cout << "RHIResourceCreationSmoke passed" << std::endl;
	return 0;
}
