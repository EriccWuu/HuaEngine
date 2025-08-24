#include <iostream>
#include <algorithm>
#include <HuaEngine.h>

// Entry Point - Must be included in main application file only
#include "HuaEngine/EntryPoint.h"

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/quaternion.hpp"

#include "HuaEngine/ECS/Components.h"
#include "Module/Rendering/RenderingComponent.h"
#include "HuaEngine/ECS/ScriptableEntity.h"
#include "HuaEngine/Serialization/Serialization.h"
#include "HuaEngine/Rendering/Material/Material.h"
#include "HuaEngine/Rendering/Mesh/Mesh.h"
#include "HuaEngine/Rendering/Mesh/MeshManager.h"
// #include "HuaEngine/Rendering/Mesh/MeshData.h"

// #include "HuaEngine/Test/TestReflection.h"
// #include "HuaEngine/Test/SerializationTest.h"
// #include "HuaEngine/Test/MaterialSerializationTest.h"
// #include "HuaEngine/Test/SceneSerializationTest.h"


using namespace HE;

class CustomLayer : public HE::Layer {
public:
	CustomLayer(): Layer("CumsomLayer") {
        // Initialize camera
        m_EditorCamera.reset(new EditorCamera());
        m_Scene.reset(new Scene());
        m_RenderSystem.reset(new RenderSystem(m_Scene));
	}

    void OnAttach() override {
        // Initialize camera
        m_EditorCamera.reset(new EditorCamera());
        m_Scene.reset(new Scene());
        m_RenderSystem.reset(new RenderSystem(m_Scene));

        // 初始化网格资产管理器并加载默认网格
        MeshManager::Instance().LoadDefaultMeshes();

        auto customMesh = Mesh::LoadFromFile("assets/CustomMesh.mesh");
        MeshManager::Instance().RegisterMesh("CustomSquare", customMesh);

        // 创建材质
        m_SandboxMaterial = Material::Create("SandboxMaterial");
        LoadMaterial("assets/SandboxMaterial.material", std::static_pointer_cast<Material>(m_SandboxMaterial).get());

        // Create material instance
        m_MaterialInstance = m_SandboxMaterial->CreateInstance();
        m_MaterialInstance->SetParameter("u_Color", glm::vec4(1.0f, 1.0f, 0.8f, 1.0f));

        // Create framebuffer
        FrameBufferSpecification spec;
        spec.Width = 1280;
        spec.Height = 720;
        spec.Attachments = { FrameBufferTextureFormat::RGBA8 };
        m_FrameBuffer = FrameBuffer::Create(spec);

        // 使用资产系统创建实体
        CreateEntitiesWithAssets();

        m_RenderSystem->SetFrameBuffer(m_FrameBuffer);
        m_Scene->AddSyetem(m_RenderSystem);

        // 序列化场景
        SaveSceneWithAssets();
    }

    void CreateEntitiesWithAssets() {
        // 创建第一个正方形实体（使用默认 Quad）
        m_Square = std::make_shared<Entity>(m_Scene->GetEntityManager().CreateEntity());
        m_Square->AddComponent<MeshComponent>("Quad");  // 使用资产名称
        m_Square->AddComponent<MaterialComponent>(m_MaterialInstance);
        auto& transform = m_Square->GetComponent<TransformComponent>();
        transform.Position.z -= 3.f;
        transform.Position += glm::vec3{ 0.5, 0.5, 0.0 };

        // 创建第二个正方形实体（使用自定义网格）
        auto secondMaterialInstance = m_SandboxMaterial->CreateInstance();
        secondMaterialInstance->SetParameter("u_Color", glm::vec4(0.8f, 0.4f, 0.9f, 1.0f));
        secondMaterialInstance->SetParameter("u_TextureScale", glm::vec2(2.0f, 2.0f));

        auto square = std::make_shared<Entity>(m_Scene->GetEntityManager().CreateEntity());
        square->AddComponent<MeshComponent>("CustomSquare");  // 使用自定义网格资产
        square->AddComponent<MaterialComponent>(secondMaterialInstance);
        auto& trans = square->GetComponent<TransformComponent>();
        trans.Position -= glm::vec3{0.5, 0.5, 0.0};

        auto thirdMaterialInstance = m_SandboxMaterial->CreateInstance();
        thirdMaterialInstance->SetParameter("u_Color", glm::vec4(0.8f, 0.0f, 0.9f, 1.0f));

        // 创建立方体实体
        auto cubeEntity = std::make_shared<Entity>(m_Scene->GetEntityManager().CreateEntity());
        cubeEntity->AddComponent<MeshComponent>("Cube");  // 使用默认 Cube
        cubeEntity->AddComponent<MaterialComponent>(thirdMaterialInstance);
        auto& cubeTransform = cubeEntity->GetComponent<TransformComponent>();
        cubeTransform.Position.z = -3.f;
        cubeTransform.Position += glm::vec3{-1.5, 0.0, 0.0};
        cubeTransform.Scale *= 0.5f;

        // 创建球体实体
        auto sphereEntity = std::make_shared<Entity>(m_Scene->GetEntityManager().CreateEntity());
        sphereEntity->AddComponent<MeshComponent>("Sphere");  // 使用默认 Sphere
        sphereEntity->AddComponent<MaterialComponent>(thirdMaterialInstance);
        auto& sphereTransform = sphereEntity->GetComponent<TransformComponent>();
        sphereTransform.Position.z = -3.f;
        sphereTransform.Position -= glm::vec3{ -1.5, 0.0, 0.0 };
        sphereTransform.Scale *= 0.5f;
    }

    void SaveSceneWithAssets() {
        // Serialize the created scene to assets folder
        std::string assetPath = "SandboxScene.scene";
        if (SaveScene(m_Scene.get(), assetPath)) {
            std::cout << "Sandbox scene saved successfully to: " << assetPath << std::endl;
        } else {
            std::cout << "Failed to save sandbox scene to: " << assetPath << std::endl;
        }
    }

	void OnUpdate() override {
        m_EditorCamera->OnUpdate();
        m_RenderSystem->RenderSingleCamera(*m_Scene, *m_EditorCamera);
        m_Scene->Update();
	}

	void OnGuiRender() override {
        // Simple fullscreen scene rendering without docking or menu bars
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
        
        // Ensure minimum valid size to prevent crashes when window is minimized
        if (scenePanelSize.x < 1.0f) scenePanelSize.x = 1.0f;
        if (scenePanelSize.y < 1.0f) scenePanelSize.y = 1.0f;
        
        glm::vec2 newSize = { scenePanelSize.x, scenePanelSize.y };
        if (newSize != m_SceneViewportSize && scenePanelSize.x > 0 && scenePanelSize.y > 0)
        {
            m_FrameBuffer->Resize((uint32_t)scenePanelSize.x, (uint32_t)scenePanelSize.y);
            m_SceneViewportSize = newSize;
        }
        
        // Only render image if we have valid dimensions
        if (m_SceneViewportSize.x > 0 && m_SceneViewportSize.y > 0)
        {
            ImGui::Image(m_FrameBuffer->GetColorAttachment(), 
                { m_SceneViewportSize.x , m_SceneViewportSize.y },
                {0, 1}, {1, 0});
        }
            
        ImGui::End();
        ImGui::PopStyleVar(3);
	}

    void OnEvent(Event& event) override {
        auto dispatcher = EventDispatcher(event);
        dispatcher.Dispatch<WindowResizeEvent>(BIND_EVENT_FUNC(CustomLayer::OnWindowResize));
    }

    void ShowLoadedMeshes() {
        auto meshNames = MeshManager::Instance().GetLoadedMeshNames();
        std::cout << "Currently loaded meshes (" << meshNames.size() << "):" << std::endl;
        for (const auto& name : meshNames) {
            auto mesh = MeshManager::Instance().GetMesh(name);
            std::cout << "  - " << name << " (GPU loaded: " << (mesh->IsLoadedToGPU() ? "Yes" : "No") << ")" << std::endl;
        }
    }

    bool OnWindowResize(WindowResizeEvent& event) {
        // Ensure minimum valid size to prevent crashes when window is minimized
        auto eventWidth = event.GetWidth();
        auto eventHeight = event.GetHeight();
        
        uint32_t width = (eventWidth > 0) ? static_cast<uint32_t>(eventWidth) : 1u;
        uint32_t height = (eventHeight > 0) ? static_cast<uint32_t>(eventHeight) : 1u;
        
        m_EditorCamera->SetViewport(width, height);
        return false;
    }

private:
    Ref<FrameBuffer> m_FrameBuffer;
    Ref<EditorCamera> m_EditorCamera;
    Ref<Entity> m_Square, m_SceneCamera;
    Ref<Scene> m_Scene;
    Ref<RenderSystem> m_RenderSystem;
    Ref<Material> m_SandboxMaterial;
    Ref<MaterialInstance> m_MaterialInstance;

    glm::vec2 m_SceneViewportSize = {0, 0};
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