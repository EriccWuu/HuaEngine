#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <string>

#include "HuaEngine.h"
#include "HuaEngine/Core/ResourcePaths.h"
#include "HuaEngine/Rendering/RenderPipeline/RenderTypes.h"
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
		Require(static_cast<bool>(sandboxMaterial->GetShader()), "Expected loaded sandbox material to have a shader");
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

	bool IsClearColor(const HE::Rendering::FrameBufferPixelRGBA8& pixel) {
		return pixel.R == 26 && pixel.G == 26 && pixel.B == 26 && pixel.A == 255;
	}

	bool HasRenderedPixel(const HE::Ref<HE::FrameBuffer>& framebuffer) {
		const auto& specification = framebuffer->GetSpecification();
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
			const auto pixel = framebuffer->ReadPixelRGBA8(0, point[0], point[1]);
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
}

int main() {
	HE::Log::Init({ .EnableConsoleOutput = false });

	SmokeApplication application;
	application.Start();

	auto& operations = application.GetOperations();
	Require(operations.Supports("rendering.attach_scene_viewport"), "Expected rendering.attach_scene_viewport to be registered");
	Require(operations.Supports("rendering.render_scene_viewport"), "Expected rendering.render_scene_viewport to be registered");

	HE::Ref<HE::Scene> scene;
	auto createScene = operations.CreateScene("RenderingSmoke", scene);
	Require(createScene.Succeeded() && scene, "Expected scene.create to succeed for rendering smoke");

	HE::FrameBufferSpecification specification;
	specification.Width = 320;
	specification.Height = 180;
	specification.Attachments = { HE::FrameBufferTextureFormat::RGBA8, HE::FrameBufferTextureFormat::DEPTH24_STENCIL8 };
	auto framebuffer = HE::FrameBuffer::Create(specification);
	Require(static_cast<bool>(framebuffer), "Expected framebuffer creation to succeed");

	HE::Rendering::EditorCamera camera;
	camera.SetViewport(320.0f, 180.0f);
	auto renderWithoutAttach = operations.RenderSceneViewport(*scene, camera);
	Require(renderWithoutAttach.Failed(), "Expected rendering.render_scene_viewport to fail before attach");

	auto attachRenderer = operations.AttachSceneViewportRenderer(scene, framebuffer);
	Require(attachRenderer.Succeeded(), "Expected rendering.attach_scene_viewport to succeed");
	Require(attachRenderer.Payload.contains("created_render_system"), "Expected rendering.attach_scene_viewport to report creation semantics");

	auto attachRendererAgain = operations.AttachSceneViewportRenderer(scene, framebuffer);
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
	Require(HasDiagnostic(initialRenderResult.Diagnostics, HE::Rendering::RenderDiagnosticCode::FallbackResourceUsed), "Expected fallback diagnostic");
	Require(renderViewport.Operation == "rendering.render_scene_viewport", "Expected render operation id to stay stable");
	Require(renderViewport.Payload.contains("render_items"), "Expected rendering.render_scene_viewport to report extracted render item count");
	Require(renderViewport.Payload.contains("submitted_items"), "Expected rendering.render_scene_viewport to report submitted item count");
	Require(renderViewport.Payload.contains("skipped_items"), "Expected rendering.render_scene_viewport to report skipped item count");
	Require(renderViewport.Payload.contains("draw_calls"), "Expected rendering.render_scene_viewport to report draw call count");
	Require(renderViewport.Payload.contains("pass_count"), "Expected rendering.render_scene_viewport to report render pass count");
	Require(renderViewport.Payload.contains("visible_items"), "Expected rendering.render_scene_viewport to report visible item count");
	Require(renderViewport.Payload.contains("diagnostics"), "Expected rendering.render_scene_viewport to report diagnostic count");
	Require(renderViewport.Payload.at("render_items") == "1", "Expected invalid renderable component triple to count as an extracted render item");
	Require(renderViewport.Payload.at("submitted_items") == "1", "Expected invalid renderable resources to submit with fallback resources");
	Require(renderViewport.Payload.at("skipped_items") == "0", "Expected invalid renderable resources to avoid skipped item count");
	Require(renderViewport.Payload.at("draw_calls") == "1", "Expected invalid renderable resources to issue a fallback draw call");
	Require(renderViewport.Payload.at("pass_count") == "1", "Expected invalid renderable resources to execute one render pass");
	Require(renderViewport.Payload.at("visible_items") == "1", "Expected invalid renderable resources to count one visible item");
	Require(renderViewport.Payload.at("diagnostics") == "2", "Expected invalid renderable resources to emit mesh and material fallback diagnostics");

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

	auto attachAssetRefSceneRenderer = operations.AttachSceneViewportRenderer(assetRefScene, framebuffer);
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
	Require(renderAssetRefScene.Payload.at("render_items") == "1", "Expected typed asset-ref scene render to extract one render item");
	Require(renderAssetRefScene.Payload.at("submitted_items") == "1", "Expected typed asset-ref scene render to submit through the asset resolver path");
	Require(renderAssetRefScene.Payload.at("skipped_items") == "0", "Expected typed asset-ref scene render to avoid skipping through the asset resolver path");
	Require(renderAssetRefScene.Payload.at("draw_calls") == "1", "Expected typed asset-ref scene render to issue one draw call through the asset resolver path");
	Require(renderAssetRefScene.Payload.at("pass_count") == "1", "Expected typed asset-ref scene render to execute one render pass");
	Require(renderAssetRefScene.Payload.at("visible_items") == "1", "Expected typed asset-ref scene render to count one visible item");
	Require(renderAssetRefScene.Payload.at("diagnostics") == "0", "Expected typed asset-ref scene render to emit no resolver diagnostics");

	HE::Ref<HE::Scene> loadedScene;
	const auto scenePath = HE::ResourcePaths::ResolveEngineResourcePath("SandboxScene.scene");
	auto loadScene = operations.LoadScene(scenePath, loadedScene);
	Require(loadScene.Succeeded() && loadedScene, "Expected sandbox scene load to succeed");
	Require(CountRenderableSubmissions(*loadedScene) == 3, "Expected loaded sandbox scene to expose three migrated builtin asset-ref renderables without project manifest fallback");

	auto attachLoadedSceneRenderer = operations.AttachSceneViewportRenderer(loadedScene, framebuffer);
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
	Require(renderLoadedScene.Payload.at("render_items") == "4", "Expected loaded sandbox scene render to extract four render items");
	Require(renderLoadedScene.Payload.at("submitted_items") == "4", "Expected loaded sandbox scene render to submit all render items through the asset resolver path");
	Require(renderLoadedScene.Payload.at("skipped_items") == "0", "Expected loaded sandbox scene render to avoid skipping render items through the asset resolver path");
	Require(renderLoadedScene.Payload.at("draw_calls") == "4", "Expected loaded sandbox scene render to issue draw calls through the asset resolver path");
	Require(renderLoadedScene.Payload.at("pass_count") == "1", "Expected loaded sandbox scene render to execute one render pass");
	Require(renderLoadedScene.Payload.at("visible_items") == "4", "Expected loaded sandbox scene render to count four visible items");
	Require(renderLoadedScene.Payload.at("diagnostics") == "1", "Expected loaded sandbox scene render to emit one fallback diagnostic for the unmigrated custom mesh");

	std::cout << "RenderingOperationsSmoke passed" << std::endl;
	return 0;
}
