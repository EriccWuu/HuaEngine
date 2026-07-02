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
				if (mesh.GetVertexArray() && material.MaterialInstance && material.MaterialInstance->GetShader()) {
					++renderableCount;
				}
			});
		return renderableCount;
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
	specification.Attachments = { HE::FrameBufferTextureFormat::RGBA8 };
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

	auto renderViewport = operations.RenderSceneViewport(*scene, camera);
	Require(renderViewport.Succeeded(), "Expected rendering.render_scene_viewport to succeed");
	Require(renderViewport.Operation == "rendering.render_scene_viewport", "Expected render operation id to stay stable");

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

	std::cout << "RenderingOperationsSmoke passed" << std::endl;
	return 0;
}
