#include "ECSTestSupport.h"
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <string>
#include <thread>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "HuaEngine.h"
#include "HuaEngine/Application/ApplicationServices.h"
#include "HuaEngine/Rendering/RenderPipeline/RenderTypes.h"
#include "HuaEngine/Rendering/RenderPipeline/RenderGraphExtension.h"
#include "HuaEngine/Rendering/RenderPipeline/RenderBindGroupBuilder.h"
#include "HuaEngine/Rendering/RenderPipeline/UniformBufferArena.h"
#include "HuaEngine/Rendering/RHI/RenderHardwareInterface.h"
#include "Module/Rendering/SceneRenderer.h"
#include "Viewport/EditorSceneRenderExtension.h"

namespace {
	class ThreadObservedGraphExtension final : public HE::Rendering::RenderGraphExtension {
	public:
		std::thread::id MainThread = std::this_thread::get_id();
		std::thread::id PassThread;
		size_t RhiCalls = 0;
		void AddBeforeOpaquePasses(HE::Rendering::RenderGraphBuilder&,
			const HE::Rendering::ForwardSceneResources&, const HE::Rendering::RenderView&) override {}
		void AddAfterOpaquePasses(HE::Rendering::RenderGraphBuilder& graph,
			const HE::Rendering::ForwardSceneResources& resources, const HE::Rendering::RenderView&) override {
			graph.AddPass("ObserveActualGraphicsThread", HE::Rendering::RenderGraphPassType::Graphics,
				[this, resources](HE::Rendering::RenderGraphPassBuilder& pass) {
					pass.WriteColor(resources.Color, HE::Rendering::LoadOp::Load, HE::Rendering::StoreOp::Store);
					pass.SetExecute([this](HE::Rendering::RenderPassContext& context) {
						PassThread = std::this_thread::get_id();
						if (PassThread != MainThread || !context.Device) throw std::runtime_error("Graphics pass escaped the main thread");
						const uint32_t data = 0;
						auto buffer = context.Device->CreateBuffer({.Usage = HE::Rendering::GpuBufferUsage::Vertex,
							.Size = sizeof(data), .Stride = sizeof(data)}, &data);
						if (!buffer) throw std::runtime_error("Observed graphics pass could not create its actual RHI buffer");
						++RhiCalls;
					});
				});
		}
	};
	class InvalidGraphExtension final : public HE::Rendering::RenderGraphExtension {
	public:
		void AddBeforeOpaquePasses(HE::Rendering::RenderGraphBuilder& graph,
			const HE::Rendering::ForwardSceneResources& resources, const HE::Rendering::RenderView&) override {
			graph.AddPass("DeliberatelyInvalidUsage", HE::Rendering::RenderGraphPassType::Graphics,
				[resources](HE::Rendering::RenderGraphPassBuilder& pass) {
					pass.Read(resources.Color, HE::Rendering::ResourceState::Undefined);
					pass.SetExecute([](HE::Rendering::RenderPassContext&) {});
				});
		}
	};
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
		HE::ApplicationServices& Services() { return GetServices(); }
	};

	uint32_t CountRenderableSubmissions(HE::Scene& scene) {
		uint32_t renderableCount = 0;
		for (const auto id : scene.GetWorld().Entities()) {
			const auto* mesh = scene.GetWorld().TryGet<HE::Rendering::MeshComponent>(id);
			const auto* material = scene.GetWorld().TryGet<HE::Rendering::MaterialComponent>(id);
			if (scene.GetWorld().Has<HE::TransformComponent>(id) && mesh && material &&
				mesh->Mesh.Reference.IsValid() && material->Material.Reference.IsValid()) ++renderableCount;
		}
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

		const auto renderingRoot = std::filesystem::path(HUAENGINE_TEST_SOURCE_ROOT) / "HuaEngine" / "src" / "HuaEngine" / "Rendering";
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
	std::cerr << "[RenderingOperationsSmoke] application started\n";
	Require(ForwardPipelineUsesExplicitVertexIndexBinding(), "Expected ForwardRenderPipeline main draw path to use explicit vertex/index binding");
	Require(SphereTrianglesFaceOutward(), "Expected generated sphere triangle winding to face outward");
	Require(CubeTrianglesFaceOutward(), "Expected generated cube triangle winding to face outward");

	auto& operations = application.GetOperations();
	Require(operations.Supports("rendering.attach_scene_viewport"), "Expected rendering.attach_scene_viewport to be registered");
	Require(operations.Supports("rendering.render_scene_viewport"), "Expected rendering.render_scene_viewport to be registered");
	const auto smokeRoot = std::filesystem::temp_directory_path() / "rendering";
	std::error_code errorCode;
	std::filesystem::remove_all(smokeRoot, errorCode);
	HE::ProjectContext projectContext;
	Require(
		operations.InitializeProject(smokeRoot / "SmokeProject", &projectContext, "RenderingSmokeProject").Succeeded(),
		"Expected rendering smoke project initialization");
	std::cerr << "[RenderingOperationsSmoke] initializing assets\n";
	Require(
		operations.InitializeProjectAssets(projectContext).Succeeded(),
		"Expected builtin render artifacts to initialize");
	std::cerr << "[RenderingOperationsSmoke] assets initialized\n";

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
	Require(attachRenderer.Payload.at("created_render_system") == "true", "Expected first attach to create a scene renderer");
	Require(attachRendererAgain.Payload.at("created_render_system") == "false", "Expected second attach to reuse the existing scene renderer");

	auto invalidRenderable = ECSTestSupport::Take(scene->CreateEntity("Invalid Renderable"));
	std::cerr << "[RenderingOperationsSmoke] entity created\n";
	auto& invalidMesh = ECSTestSupport::Add<HE::Rendering::MeshComponent>(scene->GetWorld(), invalidRenderable);
	invalidMesh.Mesh.Reference.Guid = "missing-smoke-mesh";
	auto& invalidMaterial = ECSTestSupport::Add<HE::Rendering::MaterialComponent>(scene->GetWorld(), invalidRenderable);
	invalidMaterial.Material.Reference.Guid = "missing-smoke-material";

	std::cerr << "[RenderingOperationsSmoke] initial viewport entering\n";
	auto renderViewport = operations.RenderSceneViewport(*scene, camera);
	std::cerr << "[RenderingOperationsSmoke] initial viewport returned\n";
	Require(renderViewport.Succeeded(), "Expected rendering.render_scene_viewport to succeed");
	const auto* initialRenderResultPointer = scene->LastRenderResult();
	Require(initialRenderResultPointer != nullptr, "Expected scene renderer to remain attached after viewport render");
	const auto& initialRenderResult = *initialRenderResultPointer;
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
	Require(
		std::stoull(renderViewport.Payload.at("graphics_queue_completed")) <= std::stoull(renderViewport.Payload.at("graphics_queue_signal")),
		"Expected graphics queue completion not to exceed the submitted value");
	Require(renderViewport.Payload.at("frames_in_flight") != "0", "Expected submitted forward command buffer to remain tracked in flight");
	Require(renderViewport.Payload.at("visible_items") == "1", "Expected invalid renderable resources to count one visible item");
	Require(renderViewport.Payload.at("fallback_items") == "1", "Expected invalid renderable resources to count one fallback item");
	Require(renderViewport.Payload.at("diagnostics") == "2", "Expected invalid renderable resources to emit mesh and material fallback diagnostics");
	Require(renderViewport.Payload.at("graph_resources") == "3", "Expected forward render graph to report three typed resources");
	Require(renderViewport.Payload.at("graph_edges") == "2", "Expected forward render graph to report typed dependency and output edges");
	Require(renderViewport.Payload.at("graph_outputs") == "1", "Expected forward render graph to report one output");
	Require(renderViewport.Payload.at("graph_diagnostics") == "0", "Expected forward render graph to emit no diagnostics");
	{
		ThreadObservedGraphExtension observed;
		ECSTestSupport::Check(scene->RenderSingleCamera(camera, &observed));
		Require(observed.RhiCalls == 1 && observed.PassThread == observed.MainThread,
			"Expected the actual render graph pass and its RHI call to execute on the main thread");
		std::ofstream evidence(std::filesystem::temp_directory_path() / "render-thread-evidence.json");
		evidence << "{\"schema_version\":1,\"scope\":\"RenderGraph pass execution and direct RenderDevice::CreateBuffer\","
			<< "\"main_thread\":\"" << observed.MainThread << "\",\"pass_thread\":\"" << observed.PassThread
			<< "\",\"observed_rhi_calls\":" << observed.RhiCalls << ",\"calls_on_worker\":0}";
		Require(evidence.good(), "Expected precise graphics-thread evidence persistence");
	}

    {
        std::cerr << "[RenderingOperationsSmoke] active camera selection\n";
        auto& world = scene->GetWorld();
        const auto first = ECSTestSupport::Take(scene->CreateEntity("first primary camera"));
        const auto second = ECSTestSupport::Take(scene->CreateEntity("second primary camera"));
        const auto third = ECSTestSupport::Take(scene->CreateEntity("third primary camera"));
        for (const auto id : {first, second, third}) {
            auto& component = ECSTestSupport::Add<HE::Rendering::CameraComponent>(world, id);
            component.Primary = true;
        }
        ECSTestSupport::Check(scene->OnUpdate());
        Require(scene->LastActiveCameraEntity() == first, "Expected real rendering to select the smallest active primary index");
        ECSTestSupport::Check(world.Destroy(first));
        const auto cameraType = world.Types().Find<HE::Rendering::CameraComponent>()->Id;
        ECSTestSupport::Check(world.SetComponentEnabled(second, cameraType, false));
        ECSTestSupport::Check(scene->OnUpdate());
        Require(scene->LastActiveCameraEntity() == third, "Expected camera selection to skip deleted and disabled components");
        ECSTestSupport::Check(world.SetComponentEnabled(second, cameraType, true));
        ECSTestSupport::Check(scene->OnUpdate());
        Require(scene->LastActiveCameraEntity() == second, "Expected camera re-enabling to restore minimum-index priority");
        ECSTestSupport::Check(world.SetEnabled(second, false));
        ECSTestSupport::Check(scene->OnUpdate());
        Require(scene->LastActiveCameraEntity() == third, "Expected disabled entities to be excluded from active camera selection");
        ECSTestSupport::Check(world.SetEnabled(third, false));
        ECSTestSupport::Check(scene->OnUpdate());
        Require(!scene->LastActiveCameraEntity(), "Expected no active camera when every primary candidate is disabled");
        Require(operations.RenderSceneViewport(*scene, camera).Succeeded() && !scene->LastActiveCameraEntity(),
            "Expected explicit camera rendering to remain independent of scene camera selection");
        InvalidGraphExtension invalidGraph;
        const auto failed = scene->RenderSingleCamera(camera, &invalidGraph);
        Require(!failed && failed.GetError().Code == HE::Ecs::ErrorCode::InvalidState &&
            scene->LastRenderResult() && !scene->LastRenderResult()->Succeeded,
            "Expected a failed render graph to propagate failure while retaining diagnostics");
        Require(!scene->GetWorld().Context().HasScheduledWork(), "Expected render failure to release all extraction tasks");
        Require(operations.RenderSceneViewport(*scene, camera).Succeeded(), "Expected later rendering to recover after graph rejection");
    }

	HE::Ref<HE::Scene> assetRefScene;
	auto createAssetRefScene = operations.CreateScene("TypedAssetRefSmoke", assetRefScene);
	Require(createAssetRefScene.Succeeded() && assetRefScene, "Expected typed asset-ref scene.create to succeed for rendering smoke");

	auto assetRefRenderable = ECSTestSupport::Take(assetRefScene->CreateEntity("Typed AssetRef Renderable"));
	auto& assetRefTransform = ECSTestSupport::Add<HE::TransformComponent>(assetRefScene->GetWorld(), assetRefRenderable);
	assetRefTransform.Position.z = -3.0f;
	auto& assetRefMesh = ECSTestSupport::Add<HE::Rendering::MeshComponent>(assetRefScene->GetWorld(), assetRefRenderable);
	assetRefMesh.Mesh.Reference.Guid = HE::BuiltinAssetGuids::QuadMesh;
	auto& assetRefMaterial = ECSTestSupport::Add<HE::Rendering::MaterialComponent>(assetRefScene->GetWorld(), assetRefRenderable);
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
	{
		std::cerr << "[RenderingOperationsSmoke] frame identity picking\n";
		HE::Scene pickScene("Frame identity picking");
		auto& world = pickScene.GetWorld();
		for (int index = 0; index < 10; ++index) ECSTestSupport::Take(pickScene.CreateEntity("unrendered"));
		const auto picked = ECSTestSupport::Take(pickScene.CreateEntity("pick target"));
		ECSTestSupport::Get<HE::TransformComponent>(world, picked).Position.z = -2.0f;
		ECSTestSupport::Add<HE::Rendering::MeshComponent>(world, picked).Mesh.Reference.Guid = HE::BuiltinAssetGuids::QuadMesh;
		ECSTestSupport::Add<HE::Rendering::MaterialComponent>(world, picked).Material.Reference.Guid = HE::BuiltinAssetGuids::DefaultMaterial;
		ECSTestSupport::Take(pickScene.AttachRenderer(renderTarget, &application.Services().GetAssetResolver()));
		auto objectTarget = HE::Rendering::RenderHardwareInterface::GetDevice().CreateRenderTarget({.Specification = specification});
		HE::Editor::EditorSceneRenderExtension extension;
		Require(!extension.ResolveObjectId(0, world.Id()) && !extension.ResolveObjectId(1, world.Id()),
			"Expected empty pick tables and zero tokens to resolve no entity");
		extension.SetObjectIdTarget(objectTarget);
		ECSTestSupport::Check(pickScene.RenderSingleCamera(camera, &extension));
		std::cerr << "[RenderingOperationsSmoke] pick frame rendered\n";
		uint32_t token = 0;
		for (uint32_t y = 0; y < specification.Height && !token; y += 4) {
			for (uint32_t x = 0; x < specification.Width && !token; x += 4) {
				const auto pixel = objectTarget->ReadPixelRGBA8(0, x, y);
				const uint32_t candidate = uint32_t(pixel.R) | (uint32_t(pixel.G) << 8) | (uint32_t(pixel.B) << 16) | (uint32_t(pixel.A) << 24);
				if (extension.ResolveObjectId(candidate, world.Id()) == picked) token = candidate;
			}
		}
		Require(token != 0 && token != picked.Index + 1 && extension.ResolveObjectId(token, world.Id()) == picked,
			"Expected actual GPU tokens to resolve full entity identity independently of slot index");
		Require(!extension.ResolveObjectId(token, world.Id() + 1) && !extension.ResolveObjectId(UINT32_MAX, world.Id()),
			"Expected foreign World and out-of-range pick token rejection");
		ECSTestSupport::Check(world.Destroy(picked));
		const auto replacement = ECSTestSupport::Take(pickScene.CreateEntity("reused pick slot"));
		Require(replacement.Index == picked.Index && replacement.Generation != picked.Generation &&
			!world.IsAlive(extension.ResolveObjectId(token, world.Id())),
			"Expected a delayed pick to reject a reused slot by its original generation");
		ECSTestSupport::Get<HE::TransformComponent>(world, replacement).Position.z = -2.0f;
		ECSTestSupport::Add<HE::Rendering::MeshComponent>(world, replacement).Mesh.Reference.Guid = HE::BuiltinAssetGuids::QuadMesh;
		ECSTestSupport::Add<HE::Rendering::MaterialComponent>(world, replacement).Material.Reference.Guid = HE::BuiltinAssetGuids::DefaultMaterial;
		ECSTestSupport::Check(pickScene.RenderSingleCamera(camera, &extension));
		Require(!extension.ResolveObjectId(token, world.Id()), "Expected a delayed token never to alias a replacement rendered in the next frame");
		uint32_t replacementToken = 0;
		for (uint32_t y = 0; y < specification.Height && !replacementToken; y += 4) {
			for (uint32_t x = 0; x < specification.Width && !replacementToken; x += 4) {
				const auto pixel = objectTarget->ReadPixelRGBA8(0, x, y);
				const uint32_t candidate = uint32_t(pixel.R) | (uint32_t(pixel.G) << 8) | (uint32_t(pixel.B) << 16) | (uint32_t(pixel.A) << 24);
				if (extension.ResolveObjectId(candidate, world.Id()) == replacement) replacementToken = candidate;
			}
		}
		Require(replacementToken != 0 && replacementToken != token,
			"Expected the actual replacement draw to receive a distinct frame token");
	}

	const auto addBuiltinRenderable = [&](std::string_view name, const HE::AssetGuid& meshGuid, const glm::vec3& position) {
		auto entity = ECSTestSupport::Take(assetRefScene->CreateEntity(std::string(name)));
		auto& transform = ECSTestSupport::Add<HE::TransformComponent>(assetRefScene->GetWorld(), entity);
		transform.Position = position;
		transform.Scale = glm::vec3(0.5f);
		auto& mesh = ECSTestSupport::Add<HE::Rendering::MeshComponent>(assetRefScene->GetWorld(), entity);
		mesh.Mesh.Reference.Guid = meshGuid;
		auto& material = ECSTestSupport::Add<HE::Rendering::MaterialComponent>(assetRefScene->GetWorld(), entity);
		material.Material.Reference.Guid = HE::BuiltinAssetGuids::DefaultMaterial;
		material.Overrides.SetVec4("u_Color", glm::vec4(0.8f, 0.0f, 0.9f, 1.0f));
	};
	addBuiltinRenderable("Serialized Cube", HE::BuiltinAssetGuids::CubeMesh, glm::vec3(-0.9f, 0.0f, -3.0f));
	addBuiltinRenderable("Serialized Sphere", HE::BuiltinAssetGuids::SphereMesh, glm::vec3(0.9f, 0.0f, -3.0f));

	HE::Ref<HE::Scene> loadedScene;
	std::cerr << "[RenderingOperationsSmoke] serialized scene rendering\n";
	const auto scenePath = projectContext.GetAssetRootPath() / "SerializedBuiltinScene.scene";
	Require(operations.SaveScene(*assetRefScene, scenePath).Succeeded(), "Expected builtin asset-ref scene save to succeed");
	auto loadScene = operations.LoadScene(scenePath, loadedScene);
	Require(loadScene.Succeeded() && loadedScene, "Expected builtin asset-ref scene load to succeed");
	Require(CountRenderableSubmissions(*loadedScene) == 3, "Expected loaded scene to preserve three builtin asset-ref renderables");

	auto attachLoadedSceneRenderer = operations.AttachSceneViewportRenderer(loadedScene, renderTarget);
	Require(attachLoadedSceneRenderer.Succeeded(), "Expected loaded scene renderer attach to succeed");
	auto renderLoadedScene = operations.RenderSceneViewport(*loadedScene, camera);
	Require(renderLoadedScene.Succeeded(), "Expected loaded builtin scene viewport render to succeed");
	Require(renderLoadedScene.Payload.at("render_items") == "3", "Expected loaded scene render to extract three render items");
	Require(renderLoadedScene.Payload.at("submitted_items") == "3", "Expected loaded scene render to submit all render items through the asset resolver path");
	Require(renderLoadedScene.Payload.at("skipped_items") == "0", "Expected loaded scene render to avoid skipping render items");
	Require(renderLoadedScene.Payload.at("draw_calls") == "4", "Expected three loaded scene draws and one post-process draw");
	Require(renderLoadedScene.Payload.at("pass_count") == "4", "Expected loaded scene render to execute four runtime passes");
	Require(renderLoadedScene.Payload.at("visible_items") == "3", "Expected loaded scene render to count three visible items");
	Require(renderLoadedScene.Payload.at("diagnostics") == "0", "Expected loaded scene render to resolve all builtin assets");
	Require(renderLoadedScene.Payload.at("graph_resources") == "3", "Expected loaded scene render graph to report three typed resources");
	Require(renderLoadedScene.Payload.at("graph_edges") == "2", "Expected loaded scene render graph to report typed dependency and output edges");
	Require(renderLoadedScene.Payload.at("graph_outputs") == "1", "Expected loaded scene render graph to report one output");
	Require(renderLoadedScene.Payload.at("graph_diagnostics") == "0", "Expected loaded scene render to emit no render graph diagnostics");

	const auto* loadedRenderResult = loadedScene->LastRenderResult();
	Require(loadedRenderResult != nullptr, "Expected loaded scene scene renderer to remain attached");
	const auto& loadedRenderStats = loadedRenderResult->Stats;
	Require(loadedRenderStats.BindGroupLayoutCacheHits > 0, "Expected multi-item render to reuse standard bind group layouts");
	Require(loadedRenderStats.PipelineStateCacheHits > 0, "Expected multi-item render to reuse pipeline state");

	HE::ProjectContext texturedProject;
	std::cerr << "[RenderingOperationsSmoke] textured project\n";
	const auto texturedProjectRoot = smokeRoot / "TexturedProject";
	std::filesystem::copy(std::filesystem::path(HUAENGINE_TEST_SOURCE_ROOT) / "Tests" / "TestProj", texturedProjectRoot, std::filesystem::copy_options::recursive);
	Require(operations.ResolveProjectContext(texturedProjectRoot, texturedProject).Succeeded(), "Expected textured test project context");
	Require(operations.InitializeProjectAssets(texturedProject).Succeeded(), "Expected textured test project assets");
	HE::Ref<HE::Rendering::Mesh> resolvedTexturedMesh;
	Require(application.Services().GetAssetResolver().ResolveMesh("15da0d336597b40d17f6cbf870ece1ff", resolvedTexturedMesh).Succeeded(), "Expected textured mesh resolve");
	Require(resolvedTexturedMesh && resolvedTexturedMesh->GetMeshData().Layout.Elements.size() == 3, "Expected textured mesh vertex layout with generated normals");
	const auto& texturedMeshData = resolvedTexturedMesh->GetMeshData();
	const auto texturedVertexStride = texturedMeshData.Layout.Stride / sizeof(float);
	Require(texturedVertexStride >= 8 && texturedMeshData.VertexData.size() >= texturedVertexStride * 2 && texturedMeshData.VertexData[texturedVertexStride + 3] == 1.0f, "Expected textured mesh UV data");
	HE::Ref<HE::Rendering::Material> resolvedTexturedMaterial;
	Require(application.Services().GetAssetResolver().ResolveMaterial("6de06c0940c1fcd1aa64972a6eaf9f1b", resolvedTexturedMaterial).Succeeded(), "Expected textured material resolve");
	Require(resolvedTexturedMaterial && resolvedTexturedMaterial->GetShaderProgram(), "Expected textured shader program");
	const auto* resolvedTexturedParameter = resolvedTexturedMaterial->GetParameter("u_Texture");
	Require(resolvedTexturedParameter && std::get<HE::Ref<HE::Rendering::TextureResource>>(resolvedTexturedParameter->Value), "Expected textured material GPU texture parameter");
	const auto& texturedShaderDesc = resolvedTexturedMaterial->GetShaderProgram()->GetDesc();
	const auto texturedBlock = std::find_if(texturedShaderDesc.Interface.ConstantBuffers.begin(), texturedShaderDesc.Interface.ConstantBuffers.end(), [](const auto& block) { return block.Set == 1; });
	Require(texturedBlock != texturedShaderDesc.Interface.ConstantBuffers.end(), "Expected textured material block");
	const auto texturedBlockResource = std::find_if(texturedShaderDesc.Interface.Resources.begin(), texturedShaderDesc.Interface.Resources.end(), [](const auto& resource) { return resource.Type == HE::Rendering::ShaderResourceType::ConstantBuffer && resource.Set == 1; });
	Require(texturedBlockResource != texturedShaderDesc.Interface.Resources.end(), "Expected textured material block resource");
	std::vector<HE::Rendering::ShaderResourceBinding> texturedResources;
	for (const auto& resource : texturedShaderDesc.Interface.Resources) {
		if (resource.Type == HE::Rendering::ShaderResourceType::Texture2D && resource.Set == 1) texturedResources.push_back(resource);
	}
	Require(texturedResources.size() == 1, "Expected one logical texture resource");
	auto& texturedDevice = HE::Rendering::RenderHardwareInterface::GetDevice();
	HE::Rendering::UniformBufferArena texturedArena(texturedDevice, 1024);
	auto texturedLayout = HE::Rendering::CreateMaterialBindGroupLayout(texturedDevice, *texturedBlock, texturedBlockResource->StageMask, texturedResources, texturedShaderDesc.Interface.Digest);
	auto texturedGroup = HE::Rendering::CreateMaterialBindGroup(texturedDevice, texturedArena, *resolvedTexturedMaterial->CreateInstance(), *texturedBlock, texturedResources, texturedLayout);
	Require(texturedGroup && texturedGroup->GetDesc().Entries.size() == 2, "Expected texture entry in material bind group");
	const auto boundTexture = std::get<HE::Ref<HE::Rendering::TextureResource>>(texturedGroup->GetDesc().Entries[1].Value);
	std::vector<uint8_t> boundTexturePixels;
	Require(texturedDevice.ReadbackTexture(boundTexture, 0, boundTexturePixels), "Expected bound texture readback");
	Require(std::any_of(boundTexturePixels.begin(), boundTexturePixels.end(), [](uint8_t channel) { return channel < 128; }), "Expected non-white bound texture content");
	HE::Ref<HE::Scene> texturedScene;
	Require(operations.CreateScene("TexturedMaterialSmoke", texturedScene).Succeeded(), "Expected textured scene creation");
	auto texturedEntity = ECSTestSupport::Take(texturedScene->CreateEntity("Textured Quad"));
	auto& texturedTransform = ECSTestSupport::Add<HE::TransformComponent>(texturedScene->GetWorld(), texturedEntity);
	texturedTransform.Position.z = -2.0f;
	texturedTransform.Scale = glm::vec3(2.0f);
	auto& texturedMesh = ECSTestSupport::Add<HE::Rendering::MeshComponent>(texturedScene->GetWorld(), texturedEntity);
	texturedMesh.Mesh.Reference.Guid = "15da0d336597b40d17f6cbf870ece1ff";
	auto& texturedMaterial = ECSTestSupport::Add<HE::Rendering::MaterialComponent>(texturedScene->GetWorld(), texturedEntity);
	texturedMaterial.Material.Reference.Guid = "6de06c0940c1fcd1aa64972a6eaf9f1b";
	Require(operations.AttachSceneViewportRenderer(texturedScene, renderTarget).Succeeded(), "Expected textured scene renderer attach");
	const auto texturedRender = operations.RenderSceneViewport(*texturedScene, camera);
	Require(texturedRender.Succeeded() && texturedRender.Payload.at("diagnostics") == "0", "Expected textured scene render");
	const auto center = renderTarget->ReadPixelRGBA8(0, specification.Width / 2, specification.Height / 2);
	bool hasTextureVariation = false;
	for (uint32_t y = specification.Height / 4; y < specification.Height * 3 / 4; y += 8) {
		for (uint32_t x = specification.Width / 4; x < specification.Width * 3 / 4; x += 8) {
			hasTextureVariation = hasTextureVariation || !PixelNear(center, renderTarget->ReadPixelRGBA8(0, x, y), 4);
		}
	}
	auto whiteTexture = texturedDevice.CreateTexture({ .Width = 4, .Height = 4, .Format = HE::Rendering::RenderTargetTextureFormat::RGBA8, .Usage = HE::Rendering::TextureUsageSampled | HE::Rendering::TextureUsageCopyDst });
	Require(whiteTexture && texturedDevice.UploadTexture({ .Texture = whiteTexture, .Data = std::vector<uint8_t>(4 * 4 * 4, 255) }), "Expected override texture creation");
	std::filesystem::copy_file(texturedProject.GetAssetRootPath() / "textures" / "hutao.png", texturedProject.GetAssetRootPath() / "textures" / "white-override.png", std::filesystem::copy_options::overwrite_existing);
	HE::AssetHandle whiteTextureHandle = 0;
	Require(operations.RegisterTextureAsset(texturedProject, "textures/white-override.png", whiteTexture, &whiteTextureHandle).Succeeded(), "Expected override texture registration");
	HE::AssetRecord whiteTextureRecord;
	Require(operations.ResolveAsset(whiteTextureHandle, whiteTextureRecord).Succeeded(), "Expected override texture record");
	texturedMaterial.Overrides.SetVec4("u_Color", glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
	texturedMaterial.Overrides.TextureParameters["u_Texture"] = whiteTextureRecord.Guid;
	Require(operations.RenderSceneViewport(*texturedScene, camera).Succeeded(), "Expected overridden textured scene render");
	const auto overriddenCenter = renderTarget->ReadPixelRGBA8(0, specification.Width / 2, specification.Height / 2);
	Require(overriddenCenter.R > overriddenCenter.G + 32 && overriddenCenter.R > overriddenCenter.B + 32, "Expected color and texture overrides in rendered pixels");
	Require(hasTextureVariation, "Expected textured material to produce spatially varying pixels");
	std::filesystem::remove_all(smokeRoot, errorCode);
	Require(!errorCode, "Expected rendering smoke temporary project cleanup");

	std::cout << "RenderingOperationsSmoke passed" << std::endl;
	return 0;
}
