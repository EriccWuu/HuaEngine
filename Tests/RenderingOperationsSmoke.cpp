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
	Require(renderViewport.Payload.at("render_items") == "1", "Expected invalid renderable component triple to count as an extracted render item");
	Require(renderViewport.Payload.at("submitted_items") == "0", "Expected invalid renderable resources to be skipped before submission");

	PrepareSandboxAssets();

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
	Require(renderLoadedScene.Payload.at("render_items") == "4", "Expected loaded sandbox scene render to extract four render items");
	Require(renderLoadedScene.Payload.at("submitted_items") == "4", "Expected loaded sandbox scene render to submit four render items");
	Require(HasRenderedPixel(framebuffer), "Expected loaded sandbox scene render to write at least one non-clear pixel");

	std::cout << "RenderingOperationsSmoke passed" << std::endl;
	return 0;
}
