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

#include "HuaEngine/Test/SerializationTest.h"
#include "HuaEngine/Test/MaterialSerializationTest.h"
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
        // Create vertex buffer
		float squareVertices[4 * 5] = {
			-0.5f, -0.5f, -3.0f, 0.0 , 0.0,
			 0.5f, -0.5f, -3.0f, 1.0 , 0.0,
			 0.5f,  0.5f, -3.0f, 1.0 , 1.0,
			-0.5f,  0.5f, -3.0f, 0.0 , 1.0
		};

		Ref<VertexBuffer> squareVertexBuffer;
		squareVertexBuffer = VertexBuffer::Create(squareVertices, sizeof(squareVertices));

        // Create index buffer
        unsigned int squareIndices[6] = {
            0, 1, 2 , 2, 3, 0
        };

		Ref<IndexBuffer> squareIndexBuffer;
		squareIndexBuffer = IndexBuffer::Create(squareIndices, sizeof(squareIndices) / sizeof(uint32_t));

        // Create vertex array
		BufferLayout squareLayout = {
			{ ShaderDataType::Float3, "a_Position" },
			{ ShaderDataType::Float2, "a_TexCoord" }
		};

		squareVertexBuffer->SetLayout(squareLayout);

        m_SquareVA = VertexArray::Create();
		m_SquareVA->AddVertexBuffer(squareVertexBuffer);
		m_SquareVA->SetIndexBuffer(squareIndexBuffer);

        // Create shaders - Load from file
		auto shader = Shader::CreateFromFile("assets/shaders/sandbox.glsl");
		if (!shader) {
			HE_CORE_ERROR("Failed to load sandbox shader!");
		}

		auto texture = Texture2D::Create("assets/textures/hutao.png");

        // Create Material instead of using shader and texture directly
        m_SandboxMaterial = Material::Create("SandboxMaterial", MaterialType::Unlit);
        m_SandboxMaterial->SetShader(shader);
        
        // Add texture parameter to material
        m_SandboxMaterial->AddParameter(MaterialParameter(
            "u_MainTexture", 
            MaterialParameterType::Texture2D, 
            texture,
            true // isTexture
        ));

        // Add color tint parameter
        m_SandboxMaterial->AddParameter(MaterialParameter(
            "u_Color", 
            MaterialParameterType::Vec4, 
            glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), // 白色
            false
        ));

        // Add texture scale parameter
        m_SandboxMaterial->AddParameter(MaterialParameter(
            "u_TextureScale", 
            MaterialParameterType::Vec2, 
            glm::vec2(1.0f, 1.0f), // 默认不缩放
            false
        ));

        SERIALIZE_TO_FILE(*m_SandboxMaterial, "assets/sandboxMaterial.json", SerializationFormat::JSON);

        // Create material instance
        m_MaterialInstance = m_SandboxMaterial->CreateInstance();
        
        // 可以在实例中覆盖参数
        m_MaterialInstance->SetParameter("u_Color", glm::vec4(1.0f, 1.0f, 0.8f, 1.0f)); // 粉色调

        // Create framebuffer
        FrameBufferSpecification spec;
        spec.Width = 1280;
        spec.Height = 720;
        spec.Attachments = { FrameBufferTextureFormat::RGBA8 };
        m_FrameBuffer = FrameBuffer::Create(spec);

        m_Square = std::make_shared<Entity>(m_Scene->GetEntityManager().CreateEntity());
        m_Square->AddComponent<MeshComponent>(m_SquareVA);
        m_Square->AddComponent<MaterialComponent>(m_MaterialInstance);

        // 为第二个正方形创建不同的材质实例
        auto secondMaterialInstance = m_SandboxMaterial->CreateInstance();
        secondMaterialInstance->SetParameter("u_Color", glm::vec4(0.8f, 0.4f, 0.9f, 1.0f)); // 紫色调
        secondMaterialInstance->SetParameter("u_TextureScale", glm::vec2(2.0f, 2.0f)); // 2倍纹理缩放

        auto square = std::make_shared<Entity>(m_Scene->GetEntityManager().CreateEntity());
        square->AddComponent<MeshComponent>(m_SquareVA);
        square->AddComponent<MaterialComponent>(secondMaterialInstance);
        auto& trans = square->GetComponent<TransformComponent>();
        trans.Position += glm::vec3{0.5, 0.5, 0.0};

        m_RenderSystem->SetFrameBuffer(m_FrameBuffer);

        m_Scene->AddSyetem(m_RenderSystem);

        // Serialize the created scene to assets folder
        std::string assetPath = "assets/sandbox_scene.json";
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
	Ref<VertexArray> m_SquareVA;
    Ref<FrameBuffer> m_FrameBuffer;
    Ref<EditorCamera> m_EditorCamera;
    Ref<Entity> m_Square, m_SceneCamera;
    Ref<Scene> m_Scene;
    Ref<RenderSystem> m_RenderSystem;
    Ref<Material> m_SandboxMaterial;
    Ref<MaterialInstance> m_MaterialInstance;

    glm::vec2 m_SceneViewportSize = {0, 0};
};

// For Test, remember to remove this
class Person {
public:
    void Pubfunc() {
        std::cout << "Pubfunc" << std::endl;
    }
    bool Pubfunc1(int, float, std::string) {
        std::cout << "Pubfunc1" << std::endl;
        return true;
    }

    int a = 0;
    const float b = 1.0;

private:
    void Prifunc() {}
    bool Prifunc1(int, float, std::string) {}

    int m_a;
    float m_b;
};

srefl_class(Person,
    fields(
        field(a),
        field(b)
    )
)

void TestRefl() {
    TransformComponent p;
    auto typeInfo = Refl::reflect<TransformComponent>();

    typeInfo.visit_member_variables([&p](auto&& field) {
        std::cout << field.GetValue(&p) << std::endl;
    });
    
    typeInfo.visit_member_variables([&p](auto&& field) {
        field.SetValue(&p, glm::vec3(2));
    });
    
    typeInfo.visit_member_variables([&p](auto&& field) {
        std::cout << field.GetValue(&p) << std::endl;
    });
}

class SandboxApp : public HE::Application {
public:
	SandboxApp() {
		PushLayer(new CustomLayer());
        TestRefl();
	}

	~SandboxApp() {

	}
};

HE::Application* HE::CreateApplication() {
	return new SandboxApp();
}