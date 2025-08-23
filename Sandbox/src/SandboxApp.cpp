#include <iostream>
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
        // READ THIS !!!
        // TL;DR; this demo is more complicated than what most users you would normally use.
        // If we remove all options we are showcasing, this demo would become:
        //     void ShowExampleAppDockSpace()
        //     {
        //         ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
        //     }
        // In most cases you should be able to just call DockSpaceOverViewport() and ignore all the code below!
        // In this specific demo, we are not using DockSpaceOverViewport() because:
        // - (1) we allow the host window to be floating/moveable instead of filling the viewport (when opt_fullscreen == false)
        // - (2) we allow the host window to have padding (when opt_padding == true)
        // - (3) we expose many flags and need a way to have them visible.
        // - (4) we have a local menu bar in the host window (vs. you could use BeginMainMenuBar() + DockSpaceOverViewport()
        //      in your code, but we don't here because we allow the window to be floating)

        static bool enable_docking = true;

        static bool opt_fullscreen = true;
        static bool opt_padding = false;
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

        // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
        // because it would be confusing to have two docking targets within each others.
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        if (opt_fullscreen)
        {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
            window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        }
        else
        {
            dockspace_flags &= ~ImGuiDockNodeFlags_PassthruCentralNode;
        }

        // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background
        // and handle the pass-thru hole, so we ask Begin() to not render a background.
        if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
            window_flags |= ImGuiWindowFlags_NoBackground;

        // Important: note that we proceed even if Begin() returns false (aka window is collapsed).
        // This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
        // all active windows docked into it will lose their parent and become undocked.
        // We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
        // any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
        if (!opt_padding)
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("DockSpace Demo", &enable_docking, window_flags);
        if (!opt_padding)
            ImGui::PopStyleVar();

        if (opt_fullscreen)
            ImGui::PopStyleVar(2);

        // Submit the DockSpace
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
        {
            ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        }
        else
        {
            HE_ASSERT(false, "Docking is no enabled!");
        }

        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("Options"))
            {
                // Disabling fullscreen would allow the window to be moved to the front of other windows,
                // which we can't undo at the moment without finer window depth/z control.
                ImGui::MenuItem("Fullscreen", NULL, &opt_fullscreen);
                ImGui::MenuItem("Padding", NULL, &opt_padding);
                ImGui::Separator();

                if (ImGui::MenuItem("Flag: NoDockingOverCentralNode", "", (dockspace_flags & ImGuiDockNodeFlags_NoDockingOverCentralNode) != 0)) { dockspace_flags ^= ImGuiDockNodeFlags_NoDockingOverCentralNode; }
                if (ImGui::MenuItem("Flag: NoDockingSplit", "", (dockspace_flags & ImGuiDockNodeFlags_NoDockingSplit) != 0)) { dockspace_flags ^= ImGuiDockNodeFlags_NoDockingSplit; }
                if (ImGui::MenuItem("Flag: NoUndocking", "", (dockspace_flags & ImGuiDockNodeFlags_NoUndocking) != 0)) { dockspace_flags ^= ImGuiDockNodeFlags_NoUndocking; }
                if (ImGui::MenuItem("Flag: NoResize", "", (dockspace_flags & ImGuiDockNodeFlags_NoResize) != 0)) { dockspace_flags ^= ImGuiDockNodeFlags_NoResize; }
                if (ImGui::MenuItem("Flag: AutoHideTabBar", "", (dockspace_flags & ImGuiDockNodeFlags_AutoHideTabBar) != 0)) { dockspace_flags ^= ImGuiDockNodeFlags_AutoHideTabBar; }
                if (ImGui::MenuItem("Flag: PassthruCentralNode", "", (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode) != 0, opt_fullscreen)) { dockspace_flags ^= ImGuiDockNodeFlags_PassthruCentralNode; }
                ImGui::Separator();

                if (ImGui::MenuItem("Close", NULL, false, &enable_docking != NULL))
                    enable_docking = false;
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        ImGui::End();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
        ImGui::Begin("Scene");
        ImVec2 scenePanelSize = ImGui::GetContentRegionAvail();
        if (glm::vec2{ scenePanelSize.x, scenePanelSize.y } != m_SceneViewportSize)
            m_FrameBuffer->Resize((uint32_t)scenePanelSize.x, (uint32_t)scenePanelSize.y);
        m_SceneViewportSize = { scenePanelSize.x, scenePanelSize.y };
        ImGui::Image(m_FrameBuffer->GetColorAttachment(), 
            { m_SceneViewportSize.x , m_SceneViewportSize.y },
            {0, 1}, {1, 0});
        ImGui::End();
        ImGui::PopStyleVar();
	}

    void OnEvent(Event& event) override {
        auto dispatcher = EventDispatcher(event);
        dispatcher.Dispatch<WindowResizeEvent>(BIND_EVENT_FUNC(CustomLayer::OnWindowResize));
    }

    bool OnWindowResize(WindowResizeEvent& event) {
        m_EditorCamera->SetViewport(event.GetWidth(), event.GetHeight());
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