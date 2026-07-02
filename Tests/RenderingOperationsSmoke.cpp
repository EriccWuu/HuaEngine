#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "HuaEngine.h"
#include "HuaEngine/Core/ResourcePaths.h"

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
				const auto baseMaterial = material.MaterialInstance ? material.MaterialInstance->GetBaseMaterial() : nullptr;
				if (mesh.GetVertexArray() && baseMaterial && baseMaterial->GetShader()) {
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
	invalidRenderable.AddComponent<HE::Rendering::MeshComponent>("MissingSmokeMesh");
	invalidRenderable.AddComponent<HE::Rendering::MaterialComponent>();

	auto renderViewport = operations.RenderSceneViewport(*scene, camera);
	Require(renderViewport.Succeeded(), "Expected rendering.render_scene_viewport to succeed");
	Require(renderViewport.Operation == "rendering.render_scene_viewport", "Expected render operation id to stay stable");
	Require(renderViewport.Payload.contains("render_items"), "Expected rendering.render_scene_viewport to report extracted render item count");
	Require(renderViewport.Payload.contains("submitted_items"), "Expected rendering.render_scene_viewport to report submitted item count");
	Require(renderViewport.Payload.contains("skipped_items"), "Expected rendering.render_scene_viewport to report skipped item count");
	Require(renderViewport.Payload.contains("draw_calls"), "Expected rendering.render_scene_viewport to report draw call count");
	Require(renderViewport.Payload.contains("pass_count"), "Expected rendering.render_scene_viewport to report render pass count");
	Require(renderViewport.Payload.contains("visible_items"), "Expected rendering.render_scene_viewport to report visible item count");
	Require(renderViewport.Payload.contains("diagnostics"), "Expected rendering.render_scene_viewport to report diagnostic count");
	Require(renderViewport.Payload.at("render_items") == "1", "Expected invalid renderable component triple to count as an extracted render item");
	Require(renderViewport.Payload.at("submitted_items") == "0", "Expected invalid renderable resources to be skipped before submission");
	Require(renderViewport.Payload.at("skipped_items") == "1", "Expected invalid renderable resources to increment skipped item count");
	Require(renderViewport.Payload.at("draw_calls") == "0", "Expected invalid renderable resources to issue no draw calls");
	Require(renderViewport.Payload.at("pass_count") == "1", "Expected invalid renderable resources to execute one render pass");
	Require(renderViewport.Payload.at("visible_items") == "1", "Expected invalid renderable resources to count one visible item");
	Require(renderViewport.Payload.at("diagnostics") == "1", "Expected invalid renderable resources to emit one diagnostic");

	PrepareSandboxAssets();

	HE::Ref<HE::Scene> runtimeMeshScene;
	auto createRuntimeMeshScene = operations.CreateScene("RuntimeVertexArraySmoke", runtimeMeshScene);
	Require(createRuntimeMeshScene.Succeeded() && runtimeMeshScene, "Expected runtime vertex-array scene.create to succeed for rendering smoke");

	auto runtimeMesh = HE::Rendering::MeshManager::Instance().GetMesh("CustomSquare");
	Require(static_cast<bool>(runtimeMesh), "Expected CustomSquare mesh to be registered for runtime vertex-array smoke");
	auto runtimeVertexArray = runtimeMesh->GetVertexArray();
	Require(static_cast<bool>(runtimeVertexArray), "Expected CustomSquare mesh to provide a vertex array for runtime vertex-array smoke");

	auto runtimeMaterial = HE::Rendering::MaterialLibrary::Instance().GetMaterial("SandboxMaterial");
	Require(static_cast<bool>(runtimeMaterial), "Expected SandboxMaterial to be registered for runtime vertex-array smoke");
	Require(static_cast<bool>(runtimeMaterial->GetShader()), "Expected SandboxMaterial to provide a shader for runtime vertex-array smoke");

	auto runtimeRenderable = runtimeMeshScene->GetWorld().CreateEntity("Runtime VertexArray Renderable");
	runtimeRenderable.AddComponent<HE::Rendering::MeshComponent>(runtimeVertexArray);
	runtimeRenderable.AddComponent<HE::Rendering::MaterialComponent>(runtimeMaterial->CreateInstance());

	auto attachRuntimeMeshSceneRenderer = operations.AttachSceneViewportRenderer(runtimeMeshScene, framebuffer);
	Require(attachRuntimeMeshSceneRenderer.Succeeded(), "Expected runtime vertex-array scene renderer attach to succeed");
	auto renderRuntimeMeshScene = operations.RenderSceneViewport(*runtimeMeshScene, camera);
	Require(renderRuntimeMeshScene.Succeeded(), "Expected runtime vertex-array scene viewport render to succeed");
	Require(renderRuntimeMeshScene.Payload.contains("render_items"), "Expected runtime vertex-array scene render to report extracted render item count");
	Require(renderRuntimeMeshScene.Payload.contains("submitted_items"), "Expected runtime vertex-array scene render to report submitted item count");
	Require(renderRuntimeMeshScene.Payload.contains("skipped_items"), "Expected runtime vertex-array scene render to report skipped item count");
	Require(renderRuntimeMeshScene.Payload.contains("draw_calls"), "Expected runtime vertex-array scene render to report draw call count");
	Require(renderRuntimeMeshScene.Payload.contains("pass_count"), "Expected runtime vertex-array scene render to report render pass count");
	Require(renderRuntimeMeshScene.Payload.contains("visible_items"), "Expected runtime vertex-array scene render to report visible item count");
	Require(renderRuntimeMeshScene.Payload.contains("diagnostics"), "Expected runtime vertex-array scene render to report diagnostic count");
	Require(renderRuntimeMeshScene.Payload.at("render_items") == "1", "Expected runtime vertex-array scene render to extract one render item");
	Require(renderRuntimeMeshScene.Payload.at("submitted_items") == "1", "Expected runtime vertex-array scene render to submit one render item");
	Require(renderRuntimeMeshScene.Payload.at("skipped_items") == "0", "Expected runtime vertex-array scene render to skip no render items");
	Require(renderRuntimeMeshScene.Payload.at("draw_calls") == "1", "Expected runtime vertex-array scene render to issue one draw call");
	Require(renderRuntimeMeshScene.Payload.at("pass_count") == "1", "Expected runtime vertex-array scene render to execute one render pass");
	Require(renderRuntimeMeshScene.Payload.at("visible_items") == "1", "Expected runtime vertex-array scene render to count one visible item");
	Require(renderRuntimeMeshScene.Payload.at("diagnostics") == "0", "Expected runtime vertex-array scene render to emit no diagnostics");

	HE::Ref<HE::Scene> loadedScene;
	const auto scenePath = HE::ResourcePaths::ResolveEngineResourcePath("SandboxScene.scene");
	auto loadScene = operations.LoadScene(scenePath, loadedScene);
	Require(loadScene.Succeeded() && loadedScene, "Expected sandbox scene load to succeed");
	Require(CountRenderableSubmissions(*loadedScene) == 4, "Expected loaded sandbox scene to expose four renderable submissions");

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
	Require(renderLoadedScene.Payload.at("submitted_items") == "4", "Expected loaded sandbox scene render to submit four render items");
	Require(renderLoadedScene.Payload.at("skipped_items") == "0", "Expected loaded sandbox scene render to skip no render items");
	Require(renderLoadedScene.Payload.at("draw_calls") == "4", "Expected loaded sandbox scene render to issue four draw calls");
	Require(renderLoadedScene.Payload.at("pass_count") == "1", "Expected loaded sandbox scene render to execute one render pass");
	Require(renderLoadedScene.Payload.at("visible_items") == "4", "Expected loaded sandbox scene render to count four visible items");
	Require(renderLoadedScene.Payload.at("diagnostics") == "0", "Expected loaded sandbox scene render to emit no diagnostics");
	Require(HasRenderedPixel(framebuffer), "Expected loaded sandbox scene render to write at least one non-clear pixel");

	std::cout << "RenderingOperationsSmoke passed" << std::endl;
	return 0;
}
