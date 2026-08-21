#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "HuaEngine.h"
#include "HuaEngine/Core/ResourcePaths.h"
#include "HuaEngine/Rendering/RenderPipeline/RenderTypes.h"
#include "HuaEngine/Rendering/RHI/RenderHardwareInterface.h"
#include "Module/Rendering/RenderSystem.h"

namespace {
	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[RenderingOperationsSmoke] " << message << std::endl;
			std::exit(1);
		}
	}

	HE::ApplicationSpecification MakeApplicationSpecification() {
		HE::ApplicationSpecification specification;
		specification.Name = "RenderingOperationsSmoke";
		specification.EnableGuiLayer = false;
		specification.EnableWindow = true;
		return specification;
	}

	class SmokeApplication final : public HE::Application {
	public:
		SmokeApplication()
			: HE::Application(MakeApplicationSpecification()) {}
	};

	void PrepareSandboxAssets() {
		HE::Rendering::MeshManager::Instance().LoadDefaultMeshes();

		const auto customMeshPath = HE::ResourcePaths::ResolveEngineResourcePath("CustomMesh.mesh");
		auto customMesh = HE::Rendering::Mesh::LoadFromFile(customMeshPath.generic_string());
		Require(static_cast<bool>(customMesh), "Expected CustomMesh.mesh to load for renderable scene smoke");
		HE::Rendering::MeshManager::Instance().RegisterMesh("CustomSquare", customMesh);

		auto sandboxMaterial = HE::Rendering::Material::Create("SandboxMaterial", HE::Rendering::MaterialType::Custom);
		const auto materialPath = HE::ResourcePaths::ResolveEngineResourcePath("SandboxMaterial.material");
		Require(HE::Serialization::LoadMaterial(materialPath.generic_string(), *sandboxMaterial), "Expected SandboxMaterial.material to load for renderable scene smoke");
		Require(static_cast<bool>(sandboxMaterial->GetShaderProgram()), "Expected loaded sandbox material to have a shader program");
		HE::Rendering::MaterialLibrary::Instance().RegisterMaterial(sandboxMaterial->GetName(), sandboxMaterial);
	}

	uint32_t CountRenderableSubmissions(HE::Scene& scene) {
		uint32_t renderableCount = 0;
		scene.GetWorld().Query<HE::TransformComponent, HE::Rendering::MeshComponent, HE::Rendering::MaterialComponent>().ForEach(
			[&](HE::Entity, HE::TransformComponent&, HE::Rendering::MeshComponent& mesh, HE::Rendering::MaterialComponent& material) {
				if (mesh.Mesh.Reference.IsValid() && material.Material.Reference.IsValid()) {
					++renderableCount;
				}
			});
		return renderableCount;
	}

	bool PixelNear(
		const HE::Rendering::RenderTargetPixelRGBA8& pixel,
		const HE::Rendering::RenderTargetPixelRGBA8& expected,
		uint8_t tolerance) {
		const auto nearChannel = [tolerance](uint8_t actual, uint8_t target) {
			const int delta = static_cast<int>(actual) - static_cast<int>(target);
			return delta >= -static_cast<int>(tolerance) && delta <= static_cast<int>(tolerance);
		};

		return nearChannel(pixel.R, expected.R)
			&& nearChannel(pixel.G, expected.G)
			&& nearChannel(pixel.B, expected.B)
			&& nearChannel(pixel.A, expected.A);
	}

	bool IsClearColor(const HE::Rendering::RenderTargetPixelRGBA8& pixel) {
		const HE::Rendering::RenderTargetPixelRGBA8 expectedClearColor{ 26, 26, 26, 255 };
		return PixelNear(pixel, expectedClearColor, 1);
	}

	bool HasRenderedPixel(const HE::Ref<HE::RenderTarget>& renderTarget) {
		const auto& specification = renderTarget->GetSpecification();
		const uint32_t width = specification.Width;
		const uint32_t height = specification.Height;
		const uint32_t samplePoints[][2] = {
			{ width / 2, height / 2 },
			{ width / 4, height / 4 },
			{ width / 2, height / 4 },
			{ (width * 3) / 4, height / 4 },
			{ width / 4, height / 2 },
			{ (width * 3) / 4, height / 2 },
			{ width / 4, (height * 3) / 4 },
			{ width / 2, (height * 3) / 4 },
			{ (width * 3) / 4, (height * 3) / 4 }
		};

		for (const auto& point : samplePoints) {
			const auto pixel = renderTarget->ReadPixelRGBA8(0, point[0], point[1]);
			if (!IsClearColor(pixel)) {
				return true;
			}
		}

		return false;
	}

	bool HasDiagnostic(
		const std::vector<HE::Rendering::RenderDiagnostic>& diagnostics,
		HE::Rendering::RenderDiagnosticCode code) {
		return std::any_of(diagnostics.begin(), diagnostics.end(), [code](const auto& diagnostic) {
			return diagnostic.Code == code;
		});
	}

	uint32_t CountDiagnostics(
		const std::vector<HE::Rendering::RenderDiagnostic>& diagnostics,
		HE::Rendering::RenderDiagnosticCode code) {
		return static_cast<uint32_t>(std::count_if(diagnostics.begin(), diagnostics.end(), [code](const auto& diagnostic) {
			return diagnostic.Code == code;
		}));
	}

	bool ForwardPipelineUsesExplicitVertexIndexBinding() {
		const auto readSource = [](const std::filesystem::path& sourcePath) {
			std::ifstream source(sourcePath);
			return source.is_open()
				? std::string((std::istreambuf_iterator<char>(source)), std::istreambuf_iterator<char>())
				: std::string{};
		};

		const auto renderingRoot = std::filesystem::current_path() / "HuaEngine" / "src" / "HuaEngine" / "Rendering";
		const auto pipelineContent = readSource(renderingRoot / "RenderPipeline" / "ForwardRenderPipeline.cpp");
		const auto extensionContent = readSource(renderingRoot / "RenderPipeline" / "RenderGraphExtension.h");
		const auto opaqueContent = readSource(renderingRoot / "RenderPipeline" / "GraphPasses" / "ForwardOpaquePass.cpp");
		const auto postProcessContent = readSource(renderingRoot / "RenderPipeline" / "GraphPasses" / "PostProcessPass.cpp");
		return opaqueContent.find("SetVertexBuffer(") != std::string::npos
			&& postProcessContent.find("SetIndexBuffer(") != std::string::npos
			&& postProcessContent.find("void PostProcessPass::Execute") != std::string::npos
			&& extensionContent.find("class RenderGraphExtension") != std::string::npos
			&& pipelineContent.find("extension->AddBeforeOpaquePasses") != std::string::npos
			&& opaqueContent.find("EditorGrid") == std::string::npos
			&& pipelineContent.find("BoundRenderTarget") == std::string::npos
			&& pipelineContent.find("ClearedSceneColor") == std::string::npos
			&& pipelineContent.find("graph.AddPass(m_OpaquePass)") != std::string::npos
			&& opaqueContent.find("void ForwardOpaquePass::Setup") != std::string::npos
			&& pipelineContent.find("BindTarget") == std::string::npos
			&& pipelineContent.find("ClearTarget") == std::string::npos
			&& pipelineContent.find("UnbindTarget") == std::string::npos
			&& pipelineContent.find("context.View->Target->GetColorAttachmentTextureView") == std::string::npos
			&& pipelineContent.find("CreateCommandBuffer") != std::string::npos
			&& pipelineContent.find("GetImmediateCommandList") == std::string::npos
			&& pipelineContent.find("ViewportDepthAttachment") != std::string::npos;
	}

	bool SphereTrianglesFaceOutward() {
		const auto sphere = HE::Rendering::Mesh::CreateSphere("WindingSmokeSphere", 8);
		if (!sphere) {
			return false;
		}

		const auto& meshData = sphere->GetMeshData();
		uint32_t checkedTriangleCount = 0;
		for (size_t index = 0; index + 2 < meshData.IndexData.size(); index += 3) {
			const auto readPosition = [&meshData](uint32_t vertexIndex) {
				const size_t offset = static_cast<size_t>(vertexIndex) * 5;
				return glm::vec3(
					meshData.VertexData[offset],
					meshData.VertexData[offset + 1],
					meshData.VertexData[offset + 2]);
			};

			const glm::vec3 first = readPosition(meshData.IndexData[index]);
			const glm::vec3 second = readPosition(meshData.IndexData[index + 1]);
			const glm::vec3 third = readPosition(meshData.IndexData[index + 2]);
			const glm::vec3 faceNormal = glm::cross(second - first, third - first);
			if (glm::length(faceNormal) <= 0.0001f) {
				continue;
			}

			const glm::vec3 faceCenter = (first + second + third) / 3.0f;
			if (glm::dot(faceNormal, faceCenter) <= 0.0f) {
				return false;
			}
			++checkedTriangleCount;
		}

		return checkedTriangleCount > 0;
	}

	bool CubeTrianglesFaceOutward() {
		const auto cube = HE::Rendering::Mesh::CreateCube("WindingSmokeCube");
		if (!cube) {
			return false;
		}

		const auto& meshData = cube->GetMeshData();
		uint32_t checkedTriangleCount = 0;
		for (size_t index = 0; index + 2 < meshData.IndexData.size(); index += 3) {
			const auto readPosition = [&meshData](uint32_t vertexIndex) {
				const size_t offset = static_cast<size_t>(vertexIndex) * 5;
				return glm::vec3(
					meshData.VertexData[offset],
					meshData.VertexData[offset + 1],
					meshData.VertexData[offset + 2]);
			};

			const glm::vec3 first = readPosition(meshData.IndexData[index]);
			const glm::vec3 second = readPosition(meshData.IndexData[index + 1]);
			const glm::vec3 third = readPosition(meshData.IndexData[index + 2]);
			const glm::vec3 faceNormal = glm::cross(second - first, third - first);
			const glm::vec3 faceCenter = (first + second + third) / 3.0f;
			if (glm::dot(faceNormal, faceCenter) <= 0.0f) {
				return false;
			}
			++checkedTriangleCount;
		}

		return checkedTriangleCount == 12;
	}
}

int main() {
	HE::Log::Init({ .EnableConsoleOutput = false });

	SmokeApplication application;
	application.Start();
	Require(ForwardPipelineUsesExplicitVertexIndexBinding(), "Expected ForwardRenderPipeline main draw path to use explicit vertex/index binding");
	Require(SphereTrianglesFaceOutward(), "Expected generated sphere triangle winding to face outward");
	Require(CubeTrianglesFaceOutward(), "Expected generated cube triangle winding to face outward");

	auto& operations = application.GetOperations();
	Require(operations.Supports("rendering.attach_scene_viewport"), "Expected rendering.attach_scene_viewport to be registered");
	Require(operations.Supports("rendering.render_scene_viewport"), "Expected rendering.render_scene_viewport to be registered");

	HE::Ref<HE::Scene> scene;
	auto createScene = operations.CreateScene("RenderingSmoke", scene);
	Require(createScene.Succeeded() && scene, "Expected scene.create to succeed for rendering smoke");

	HE::RenderTargetSpecification specification;
	specification.Width = 320;
	specification.Height = 180;
	specification.Attachments = { HE::RenderTargetTextureFormat::RGBA8, HE::RenderTargetTextureFormat::DEPTH24_STENCIL8 };
	auto renderTarget = HE::Rendering::RenderHardwareInterface::GetDevice().CreateRenderTarget({ .Specification = specification });
	Require(static_cast<bool>(renderTarget), "Expected render target creation to succeed");

	const auto camera = HE::Rendering::RenderCamera(
		glm::perspective(glm::radians(45.0f), 320.0f / 180.0f, 0.1f, 100.0f),
		glm::mat4(1.0f));
	auto renderWithoutAttach = operations.RenderSceneViewport(*scene, camera);
	Require(renderWithoutAttach.Failed(), "Expected rendering.render_scene_viewport to fail before attach");

	auto attachRenderer = operations.AttachSceneViewportRenderer(scene, renderTarget);
	Require(attachRenderer.Succeeded(), "Expected rendering.attach_scene_viewport to succeed");
	Require(attachRenderer.Payload.contains("created_render_system"), "Expected rendering.attach_scene_viewport to report creation semantics");

	auto attachRendererAgain = operations.AttachSceneViewportRenderer(scene, renderTarget);
	Require(attachRendererAgain.Succeeded(), "Expected rendering.attach_scene_viewport to support reuse");
	Require(attachRenderer.Payload.at("created_render_system") == "true", "Expected first attach to create a render system");
	Require(attachRendererAgain.Payload.at("created_render_system") == "false", "Expected second attach to reuse the existing render system");

	auto invalidRenderable = scene->GetWorld().CreateEntity("Invalid Renderable");
	auto& invalidMesh = invalidRenderable.AddComponent<HE::Rendering::MeshComponent>();
	invalidMesh.Mesh.Reference.Guid = "missing-smoke-mesh";
	auto& invalidMaterial = invalidRenderable.AddComponent<HE::Rendering::MaterialComponent>();
	invalidMaterial.Material.Reference.Guid = "missing-smoke-material";

	auto renderViewport = operations.RenderSceneViewport(*scene, camera);
	Require(renderViewport.Succeeded(), "Expected rendering.render_scene_viewport to succeed");
	auto initialRenderSystem = scene->FindSystem<HE::RenderSystem>();
	Require(static_cast<bool>(initialRenderSystem), "Expected render system to remain attached after viewport render");
	const auto& initialRenderResult = initialRenderSystem->GetLastRenderResult();
	Require(initialRenderResult.Succeeded, "Expected render result to succeed with fallback resources");
	Require(initialRenderResult.Stats.SkippedItems == 0, "Expected fallback resources to avoid skipping render item");
	Require(initialRenderResult.Stats.FallbackItems == 1, "Expected one render item with fallback resources even when mesh and material both fall back");
	Require(HasDiagnostic(initialRenderResult.Diagnostics, HE::Rendering::RenderDiagnosticCode::FallbackResourceUsed), "Expected fallback diagnostic");
	Require(CountDiagnostics(initialRenderResult.Diagnostics, HE::Rendering::RenderDiagnosticCode::FallbackResourceUsed) >= 2, "Expected separate fallback diagnostics for missing mesh and material");
	Require(renderViewport.Operation == "rendering.render_scene_viewport", "Expected render operation id to stay stable");
	Require(renderViewport.Payload.contains("render_items"), "Expected rendering.render_scene_viewport to report extracted render item count");
	Require(renderViewport.Payload.contains("submitted_items"), "Expected rendering.render_scene_viewport to report submitted item count");
	Require(renderViewport.Payload.contains("skipped_items"), "Expected rendering.render_scene_viewport to report skipped item count");
	Require(renderViewport.Payload.contains("draw_calls"), "Expected rendering.render_scene_viewport to report draw call count");
	Require(renderViewport.Payload.contains("pass_count"), "Expected rendering.render_scene_viewport to report render pass count");
	Require(renderViewport.Payload.contains("graphics_queue_signal"), "Expected rendering.render_scene_viewport to report graphics queue signal value");
	Require(renderViewport.Payload.contains("graphics_queue_completed"), "Expected rendering.render_scene_viewport to report graphics queue completed value");
	Require(renderViewport.Payload.contains("frames_in_flight"), "Expected rendering.render_scene_viewport to report frames in flight");
	Require(renderViewport.Payload.contains("visible_items"), "Expected rendering.render_scene_viewport to report visible item count");
	Require(renderViewport.Payload.contains("fallback_items"), "Expected rendering.render_scene_viewport to report fallback item count");
	Require(renderViewport.Payload.contains("diagnostics"), "Expected rendering.render_scene_viewport to report diagnostic count");
	Require(renderViewport.Payload.contains("graph_resources"), "Expected rendering.render_scene_viewport to report render graph resource count");
	Require(renderViewport.Payload.contains("graph_edges"), "Expected rendering.render_scene_viewport to report render graph edge count");
	Require(renderViewport.Payload.contains("graph_outputs"), "Expected rendering.render_scene_viewport to report render graph output count");
	Require(renderViewport.Payload.contains("graph_diagnostics"), "Expected rendering.render_scene_viewport to report render graph diagnostic count");
	Require(renderViewport.Payload.at("render_items") == "1", "Expected invalid renderable component triple to count as an extracted render item");
	Require(renderViewport.Payload.at("submitted_items") == "1", "Expected invalid renderable resources to submit with fallback resources");
	Require(renderViewport.Payload.at("skipped_items") == "0", "Expected invalid renderable resources to avoid skipped item count");
	Require(renderViewport.Payload.at("draw_calls") == "2", "Expected fallback draw and post-process draw without editor extensions");
	Require(renderViewport.Payload.at("pass_count") == "4", "Expected runtime render graph to execute four render passes");
	Require(renderViewport.Payload.at("graphics_queue_signal") != "0", "Expected invalid renderable resources to submit a graphics command buffer");
	Require(renderViewport.Payload.at("graphics_queue_completed") == renderViewport.Payload.at("graphics_queue_signal"), "Expected graphics queue fence to complete submitted value");
	Require(renderViewport.Payload.at("frames_in_flight") != "0", "Expected submitted forward command buffer to remain tracked in flight");
	Require(renderViewport.Payload.at("visible_items") == "1", "Expected invalid renderable resources to count one visible item");
	Require(renderViewport.Payload.at("fallback_items") == "1", "Expected invalid renderable resources to count one fallback item");
	Require(renderViewport.Payload.at("diagnostics") == "2", "Expected invalid renderable resources to emit mesh and material fallback diagnostics");
	Require(renderViewport.Payload.at("graph_resources") == "3", "Expected forward render graph to report three typed resources");
	Require(renderViewport.Payload.at("graph_edges") == "2", "Expected forward render graph to report typed dependency and output edges");
	Require(renderViewport.Payload.at("graph_outputs") == "1", "Expected forward render graph to report one output");
	Require(renderViewport.Payload.at("graph_diagnostics") == "0", "Expected forward render graph to emit no diagnostics");

	PrepareSandboxAssets();

	HE::Ref<HE::Scene> assetRefScene;
	auto createAssetRefScene = operations.CreateScene("TypedAssetRefSmoke", assetRefScene);
	Require(createAssetRefScene.Succeeded() && assetRefScene, "Expected typed asset-ref scene.create to succeed for rendering smoke");

	auto assetRefRenderable = assetRefScene->GetWorld().CreateEntity("Typed AssetRef Renderable");
	auto& assetRefTransform = assetRefRenderable.AddComponent<HE::TransformComponent>();
	assetRefTransform.Position.z = -3.0f;
	auto& assetRefMesh = assetRefRenderable.AddComponent<HE::Rendering::MeshComponent>();
	assetRefMesh.Mesh.Reference.Guid = HE::BuiltinAssetGuids::QuadMesh;
	auto& assetRefMaterial = assetRefRenderable.AddComponent<HE::Rendering::MaterialComponent>();
	assetRefMaterial.Material.Reference.Guid = HE::BuiltinAssetGuids::DefaultMaterial;
	assetRefMaterial.Overrides.SetVec4("u_Color", glm::vec4(0.9f, 0.8f, 0.2f, 1.0f));
	Require(!assetRefMaterial.Overrides.Empty(), "Expected typed asset-ref renderable to carry material overrides");

	auto attachAssetRefSceneRenderer = operations.AttachSceneViewportRenderer(assetRefScene, renderTarget);
	Require(attachAssetRefSceneRenderer.Succeeded(), "Expected typed asset-ref scene renderer attach to succeed");
	auto renderAssetRefScene = operations.RenderSceneViewport(*assetRefScene, camera);
	Require(renderAssetRefScene.Succeeded(), "Expected typed asset-ref scene viewport render to succeed");
	Require(renderAssetRefScene.Payload.contains("render_items"), "Expected typed asset-ref scene render to report extracted render item count");
	Require(renderAssetRefScene.Payload.contains("submitted_items"), "Expected typed asset-ref scene render to report submitted item count");
	Require(renderAssetRefScene.Payload.contains("skipped_items"), "Expected typed asset-ref scene render to report skipped item count");
	Require(renderAssetRefScene.Payload.contains("draw_calls"), "Expected typed asset-ref scene render to report draw call count");
	Require(renderAssetRefScene.Payload.contains("pass_count"), "Expected typed asset-ref scene render to report render pass count");
	Require(renderAssetRefScene.Payload.contains("visible_items"), "Expected typed asset-ref scene render to report visible item count");
	Require(renderAssetRefScene.Payload.contains("diagnostics"), "Expected typed asset-ref scene render to report diagnostic count");
	Require(renderAssetRefScene.Payload.contains("graph_resources"), "Expected typed asset-ref scene render to report render graph resource count");
	Require(renderAssetRefScene.Payload.contains("graph_edges"), "Expected typed asset-ref scene render to report render graph edge count");
	Require(renderAssetRefScene.Payload.contains("graph_outputs"), "Expected typed asset-ref scene render to report render graph output count");
	Require(renderAssetRefScene.Payload.contains("graph_diagnostics"), "Expected typed asset-ref scene render to report render graph diagnostic count");
	Require(renderAssetRefScene.Payload.at("render_items") == "1", "Expected typed asset-ref scene render to extract one render item");
	Require(renderAssetRefScene.Payload.at("submitted_items") == "1", "Expected typed asset-ref scene render to submit through the asset resolver path");
	Require(renderAssetRefScene.Payload.at("skipped_items") == "0", "Expected typed asset-ref scene render to avoid skipping through the asset resolver path");
	Require(renderAssetRefScene.Payload.at("draw_calls") == "2", "Expected typed asset draw and post-process draw without editor extensions");
	Require(renderAssetRefScene.Payload.at("pass_count") == "4", "Expected typed asset-ref scene render to execute four runtime passes");
	Require(renderAssetRefScene.Payload.at("visible_items") == "1", "Expected typed asset-ref scene render to count one visible item");
	Require(renderAssetRefScene.Payload.at("diagnostics") == "0", "Expected typed asset-ref scene render to emit no resolver diagnostics");
	Require(renderAssetRefScene.Payload.at("graph_resources") == "3", "Expected typed asset-ref scene render graph to report three typed resources");
	Require(renderAssetRefScene.Payload.at("graph_edges") == "2", "Expected typed asset-ref scene render graph to report typed dependency and output edges");
	Require(renderAssetRefScene.Payload.at("graph_outputs") == "1", "Expected typed asset-ref scene render graph to report one output");
	Require(renderAssetRefScene.Payload.at("graph_diagnostics") == "0", "Expected typed asset-ref scene render to emit no render graph diagnostics");
	Require(HasRenderedPixel(renderTarget), "Expected typed asset-ref render path to write a non-clear render target pixel");
	const HE::Rendering::RenderTargetPixelRGBA8 expectedOverrideColor{ 230, 204, 51, 255 };
	const auto& typedSpec = renderTarget->GetSpecification();
	bool hasOverrideColorPixel = false;
	for (uint32_t y = typedSpec.Height / 8; y < typedSpec.Height; y += typedSpec.Height / 8) {
		for (uint32_t x = typedSpec.Width / 8; x < typedSpec.Width; x += typedSpec.Width / 8) {
			const auto pixel = renderTarget->ReadPixelRGBA8(0, x, y);
			hasOverrideColorPixel = hasOverrideColorPixel || PixelNear(pixel, expectedOverrideColor, 8);
		}
	}
	Require(hasOverrideColorPixel, "Expected typed asset-ref material override color to be visible in the render target");

	HE::Ref<HE::Scene> loadedScene;
	const auto scenePath = HE::ResourcePaths::ResolveEngineResourcePath("SandboxScene.scene");
	auto loadScene = operations.LoadScene(scenePath, loadedScene);
	Require(loadScene.Succeeded() && loadedScene, "Expected sandbox scene load to succeed");
	Require(CountRenderableSubmissions(*loadedScene) == 3, "Expected loaded sandbox scene to expose three migrated builtin asset-ref renderables without project manifest fallback");

	auto attachLoadedSceneRenderer = operations.AttachSceneViewportRenderer(loadedScene, renderTarget);
	Require(attachLoadedSceneRenderer.Succeeded(), "Expected loaded scene renderer attach to succeed");
	auto renderLoadedScene = operations.RenderSceneViewport(*loadedScene, camera);
	Require(renderLoadedScene.Succeeded(), "Expected loaded sandbox scene viewport render to succeed");
	Require(renderLoadedScene.Payload.contains("render_items"), "Expected loaded sandbox scene render to report extracted render item count");
	Require(renderLoadedScene.Payload.contains("submitted_items"), "Expected loaded sandbox scene render to report submitted item count");
	Require(renderLoadedScene.Payload.contains("skipped_items"), "Expected loaded sandbox scene render to report skipped item count");
	Require(renderLoadedScene.Payload.contains("draw_calls"), "Expected loaded sandbox scene render to report draw call count");
	Require(renderLoadedScene.Payload.contains("pass_count"), "Expected loaded sandbox scene render to report render pass count");
	Require(renderLoadedScene.Payload.contains("visible_items"), "Expected loaded sandbox scene render to report visible item count");
	Require(renderLoadedScene.Payload.contains("diagnostics"), "Expected loaded sandbox scene render to report diagnostic count");
	Require(renderLoadedScene.Payload.contains("graph_resources"), "Expected loaded sandbox scene render to report render graph resource count");
	Require(renderLoadedScene.Payload.contains("graph_edges"), "Expected loaded sandbox scene render to report render graph edge count");
	Require(renderLoadedScene.Payload.contains("graph_outputs"), "Expected loaded sandbox scene render to report render graph output count");
	Require(renderLoadedScene.Payload.contains("graph_diagnostics"), "Expected loaded sandbox scene render to report render graph diagnostic count");
	Require(renderLoadedScene.Payload.at("render_items") == "4", "Expected loaded sandbox scene render to extract four render items");
	Require(renderLoadedScene.Payload.at("submitted_items") == "4", "Expected loaded sandbox scene render to submit all render items through the asset resolver path");
	Require(renderLoadedScene.Payload.at("skipped_items") == "0", "Expected loaded sandbox scene render to avoid skipping render items through the asset resolver path");
	Require(renderLoadedScene.Payload.at("draw_calls") == "5", "Expected loaded scene draws and post-process draw without editor extensions");
	Require(renderLoadedScene.Payload.at("pass_count") == "4", "Expected loaded sandbox scene render to execute four runtime passes");
	Require(renderLoadedScene.Payload.at("visible_items") == "4", "Expected loaded sandbox scene render to count four visible items");
	Require(renderLoadedScene.Payload.at("diagnostics") == "1", "Expected loaded sandbox scene render to emit one fallback diagnostic for the unmigrated custom mesh");
	Require(renderLoadedScene.Payload.at("graph_resources") == "3", "Expected loaded sandbox scene render graph to report three typed resources");
	Require(renderLoadedScene.Payload.at("graph_edges") == "2", "Expected loaded sandbox scene render graph to report typed dependency and output edges");
	Require(renderLoadedScene.Payload.at("graph_outputs") == "1", "Expected loaded sandbox scene render graph to report one output");
	Require(renderLoadedScene.Payload.at("graph_diagnostics") == "0", "Expected loaded sandbox scene render to emit no render graph diagnostics");
	auto loadedRenderSystem = loadedScene->FindSystem<HE::RenderSystem>();
	Require(static_cast<bool>(loadedRenderSystem), "Expected loaded scene render system to remain attached");
	const auto& loadedRenderStats = loadedRenderSystem->GetLastRenderResult().Stats;
	Require(loadedRenderStats.BindGroupLayoutCacheHits > 0, "Expected multi-item render to reuse standard bind group layouts");
	Require(loadedRenderStats.PipelineStateCacheHits > 0, "Expected multi-item render to reuse pipeline state");

	std::cout << "RenderingOperationsSmoke passed" << std::endl;
	return 0;
}
