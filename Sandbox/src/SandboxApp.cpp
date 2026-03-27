#include <algorithm>
#include <iostream>
#include <unordered_map>

#include <HuaEngine.h>

// Entry Point - Must be included in main application file only
#include "HuaEngine/EntryPoint.h"

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/quaternion.hpp"

#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/ECS/ScriptableEntity.h"
#include "HuaEngine/Serialization/Serialization.h"

using namespace HE;

class CustomLayer : public HE::Layer {
public:
	CustomLayer()
		: Layer("CumsomLayer") {
		m_EditorCamera = CreateRef<Rendering::EditorCamera>();
		m_Scene = CreateRef<Scene>();
	}

	void OnAttach() override {
		m_EditorCamera = CreateRef<Rendering::EditorCamera>();
		m_Scene = CreateRef<Scene>();

		HE_CORE_INFO("Sandbox startup: preloading runtime assets");
		HE_CORE_ASSERT(EnsureSandboxAssetsLoaded(), "Sandbox assets must load before scene initialization");

		FrameBufferSpecification spec;
		spec.Width = 1280;
		spec.Height = 720;
		spec.Attachments = { FrameBufferTextureFormat::RGBA8 };
		m_FrameBuffer = FrameBuffer::Create(spec);

		HE_CORE_INFO("Sandbox startup: loading scene from assets/SandboxScene.scene");
		auto loadSceneResult = Application::GetInstance().GetOperations().LoadScene("assets/SandboxScene.scene", m_Scene);
		if (!loadSceneResult.Succeeded() || !m_Scene) {
			HE_CORE_WARN("Sandbox scene file could not be loaded, falling back to generated demo scene");
			m_Scene = CreateRef<Scene>("Sandbox");
			CreateEntitiesWithAssets();
			SaveSceneWithAssets();
		} else {
			HE_CORE_INFO("Sandbox startup: scene file loaded successfully");
			if (!WarmSceneMeshes()) {
				HE_CORE_WARN("Sandbox scene file could not be warmed for rendering, falling back to generated demo scene");
				m_Scene = CreateRef<Scene>("Sandbox");
				CreateEntitiesWithAssets();
				SaveSceneWithAssets();
			}
		}

		auto attachRenderer = Application::GetInstance().GetOperations().AttachSceneViewportRenderer(m_Scene, m_FrameBuffer);
		HE_CORE_ASSERT(attachRenderer.Succeeded(), "Sandbox scene viewport renderer must attach successfully");
		HE_CORE_INFO("Sandbox startup: scene viewport renderer attached");
	}

	void OnUpdate() override {
		m_EditorCamera->OnUpdate();
		auto renderResult = Application::GetInstance().GetOperations().RenderSceneViewport(*m_Scene, *m_EditorCamera);
		HE_CORE_ASSERT(renderResult.Succeeded(), "Sandbox scene viewport render must succeed");
		m_Scene->Update();
	}

	void OnGuiRender() override {
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);

		ImGuiWindowFlags window_flags =
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoBringToFrontOnFocus |
			ImGuiWindowFlags_NoNavFocus |
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoScrollWithMouse;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

		ImGui::Begin("Scene", nullptr, window_flags);

		ImVec2 scenePanelSize = ImGui::GetContentRegionAvail();
		if (scenePanelSize.x < 1.0f) scenePanelSize.x = 1.0f;
		if (scenePanelSize.y < 1.0f) scenePanelSize.y = 1.0f;

		glm::vec2 newSize = { scenePanelSize.x, scenePanelSize.y };
		if (newSize != m_SceneViewportSize && scenePanelSize.x > 0 && scenePanelSize.y > 0) {
			m_FrameBuffer->Resize((uint32_t)scenePanelSize.x, (uint32_t)scenePanelSize.y);
			m_SceneViewportSize = newSize;
		}

		if (m_SceneViewportSize.x > 0 && m_SceneViewportSize.y > 0) {
			ImGui::Image(
				m_FrameBuffer->GetColorAttachment(),
				{ m_SceneViewportSize.x , m_SceneViewportSize.y },
				{ 0, 1 }, { 1, 0 });
		}

		ImGui::End();
		ImGui::PopStyleVar(3);
	}

	void OnEvent(Event& event) override {
		auto dispatcher = EventDispatcher(event);
		dispatcher.Dispatch<WindowResizeEvent>(BIND_EVENT_FUNC(CustomLayer::OnWindowResize));
	}

	bool OnWindowResize(WindowResizeEvent& event) {
		auto eventWidth = event.GetWidth();
		auto eventHeight = event.GetHeight();

		uint32_t width = (eventWidth > 0) ? static_cast<uint32_t>(eventWidth) : 1u;
		uint32_t height = (eventHeight > 0) ? static_cast<uint32_t>(eventHeight) : 1u;

		m_EditorCamera->SetViewport(width, height);
		return false;
	}

private:
	bool EnsureSandboxAssetsLoaded() {
		m_RuntimeMeshes.clear();
		m_RuntimeMeshes["Quad"] = Mesh::CreateQuad("Quad");
		m_RuntimeMeshes["Cube"] = Mesh::CreateCube("Cube");
		m_RuntimeMeshes["Sphere"] = Mesh::CreateSphere("Sphere", 32);

		auto customMesh = Mesh::LoadFromFile("assets/CustomMesh.mesh");
		if (!customMesh) {
			HE_CORE_ERROR("Failed to load assets/CustomMesh.mesh");
			return false;
		}
		m_RuntimeMeshes["CustomSquare"] = customMesh;

		m_SandboxMaterial = Material::Create("SandboxMaterial");
		if (!Serialization::LoadMaterial("assets/SandboxMaterial.material", *m_SandboxMaterial)) {
			HE_CORE_ERROR("Failed to load assets/SandboxMaterial.material");
			return false;
		}
		Rendering::MaterialLibrary::Instance().RegisterMaterial(m_SandboxMaterial->GetName(), m_SandboxMaterial);

		m_MaterialInstance = m_SandboxMaterial->CreateInstance();
		m_MaterialInstance->SetParameter("u_Color", glm::vec4(1.0f, 1.0f, 0.8f, 1.0f));
		return true;
	}

	bool WarmSceneMeshes() {
		auto& registry = m_Scene->GetEntityManager().GetRegistry();
		auto meshView = registry.view<Rendering::MeshComponent>();
		bool success = true;
		for (auto entityHandle : meshView) {
			auto& meshComponent = registry.get<Rendering::MeshComponent>(entityHandle);
			const auto meshIt = m_RuntimeMeshes.find(meshComponent.MeshAssetName);
			if (meshIt == m_RuntimeMeshes.end() || !meshIt->second) {
				HE_CORE_WARN("Sandbox startup: mesh '{}' is not available for scene warm-up", meshComponent.MeshAssetName);
				success = false;
				continue;
			}

			meshComponent.SetMesh(meshIt->second);
		}
		return success;
	}

	void CreateEntitiesWithAssets() {
		m_Square = std::make_shared<Entity>(m_Scene->GetEntityManager().CreateEntity());
		m_Square->AddComponent<MeshComponent>("Quad");
		m_Square->AddComponent<MaterialComponent>(m_MaterialInstance);
		m_Square->GetComponent<MeshComponent>().SetMesh(m_RuntimeMeshes.at("Quad"));
		auto& transform = m_Square->GetComponent<TransformComponent>();
		transform.Position.z -= 3.f;
		transform.Position += glm::vec3{ 0.5f, 0.5f, 0.0f };

		auto secondMaterialInstance = m_SandboxMaterial->CreateInstance();
		secondMaterialInstance->SetParameter("u_Color", glm::vec4(0.8f, 0.4f, 0.9f, 1.0f));
		secondMaterialInstance->SetParameter("u_TextureScale", glm::vec2(2.0f, 2.0f));

		auto square = std::make_shared<Entity>(m_Scene->GetEntityManager().CreateEntity());
		square->AddComponent<MeshComponent>("CustomSquare");
		square->AddComponent<MaterialComponent>(secondMaterialInstance);
		square->GetComponent<MeshComponent>().SetMesh(m_RuntimeMeshes.at("CustomSquare"));
		auto& trans = square->GetComponent<TransformComponent>();
		trans.Position -= glm::vec3{ 0.5f, 0.5f, 0.0f };

		auto thirdMaterialInstance = m_SandboxMaterial->CreateInstance();
		thirdMaterialInstance->SetParameter("u_Color", glm::vec4(0.8f, 0.0f, 0.9f, 1.0f));

		auto cubeEntity = std::make_shared<Entity>(m_Scene->GetEntityManager().CreateEntity());
		cubeEntity->AddComponent<MeshComponent>("Cube");
		cubeEntity->AddComponent<MaterialComponent>(thirdMaterialInstance);
		cubeEntity->GetComponent<MeshComponent>().SetMesh(m_RuntimeMeshes.at("Cube"));
		auto& cubeTransform = cubeEntity->GetComponent<TransformComponent>();
		cubeTransform.Position.z = -3.f;
		cubeTransform.Position += glm::vec3{ -1.5f, 0.0f, 0.0f };
		cubeTransform.Scale *= 0.5f;

		auto sphereEntity = std::make_shared<Entity>(m_Scene->GetEntityManager().CreateEntity());
		sphereEntity->AddComponent<MeshComponent>("Sphere");
		sphereEntity->AddComponent<MaterialComponent>(thirdMaterialInstance);
		sphereEntity->GetComponent<MeshComponent>().SetMesh(m_RuntimeMeshes.at("Sphere"));
		auto& sphereTransform = sphereEntity->GetComponent<TransformComponent>();
		sphereTransform.Position.z = -3.f;
		sphereTransform.Position -= glm::vec3{ -1.5f, 0.0f, 0.0f };
		sphereTransform.Scale *= 0.5f;
	}

	void SaveSceneWithAssets() {
		const std::string assetPath = "assets/SandboxScene.scene";
		if (Serialization::SaveScene(*m_Scene, assetPath)) {
			std::cout << "Sandbox scene saved successfully to: " << assetPath << std::endl;
		} else {
			std::cout << "Failed to save sandbox scene to: " << assetPath << std::endl;
		}
	}

private:
	Ref<FrameBuffer> m_FrameBuffer;
	Ref<Rendering::EditorCamera> m_EditorCamera;
	Ref<Entity> m_Square, m_SceneCamera;
	Ref<Scene> m_Scene;
	Ref<Material> m_SandboxMaterial;
	Ref<MaterialInstance> m_MaterialInstance;
	std::unordered_map<std::string, Ref<Mesh>> m_RuntimeMeshes;

	glm::vec2 m_SceneViewportSize = { 0, 0 };
};

class SandboxApp : public HE::Application {
public:
	SandboxApp() {
		PushLayer(new CustomLayer());
	}

	~SandboxApp() {
	}
};

HE::Application* HE::CreateApplication() {
	return new SandboxApp();
}
