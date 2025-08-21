#include "enginepch.h"
#include "EditorLayer.h"

#include "imgui.h"

namespace HE {
    EditorLayer::EditorLayer() : Layer("EditorLayer") {
        // Initialize camera
        m_EditorCamera.reset(new EditorCamera());
        m_Scene.reset(new Scene());
        m_RenderSystem.reset(new RenderSystem(m_Scene));
        m_SceneHierarchy.reset(new SceneHierarchyPanel(m_Scene));
        m_Inspector.reset(new InspectorPanel);
        m_Concole.reset(new ConcolePanel);
    }

    void EditorLayer::OnAttach() {
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

        // Create shaders
        std::string squareVS = R"(
			#version 330 core

			layout(location = 0) in vec3 a_Position;
			layout(location = 1) in vec2 a_TexCoord;

			out vec3 v_Position;
			out vec2 v_TexCoord;

            uniform mat4 u_ViewProjection;
            uniform mat4 u_Transform;

			void main() {
				v_TexCoord = a_TexCoord;
				gl_Position = u_ViewProjection * u_Transform * vec4(a_Position, 1.0);
			}
		)";

        std::string squareFS = R"(
			#version 330 core

			out vec4 FragColor;
			in vec2 v_TexCoord;

			uniform sampler2D u_Texture;

			void main() {
				// FragColor = texture(u_Texture, v_TexCoord);
                FragColor = vec4(v_TexCoord, 0.0, 1.0);
			}
		)";

        m_SquareShader = Shader::Create(squareVS, squareFS);

        m_Texture = Texture2D::Create("assets/textures/hutao.png");

        // Create framebuffer
        FrameBufferSpecification spec;
        spec.Width = 1280;
        spec.Height = 720;
        spec.Attachments = { FrameBufferTextureFormat::RGBA8 };
        m_FrameBuffer = FrameBuffer::Create(spec);

        m_Square = std::make_shared<Entity>(m_Scene->GetEntityManager().CreateEntity());
        m_Square->AddComponent<MeshComponent>(m_SquareVA);
        m_Square->AddComponent<RendererComponent>(m_SquareShader, m_Texture);

        auto square = std::make_shared<Entity>(m_Scene->GetEntityManager().CreateEntity());
        square->AddComponent<MeshComponent>(m_SquareVA);
        square->AddComponent<RendererComponent>(m_SquareShader, m_Texture);
        auto& trans = square->GetComponent<TransformComponent>();
        trans.Position += glm::vec3{ 0.5, 0.5, 0.0 };

        auto square1 = std::make_shared<Entity>(m_Scene->GetEntityManager().CreateEntity());
        square1->AddComponent<MeshComponent>(m_SquareVA);
        square1->AddComponent<RendererComponent>(m_SquareShader, m_Texture);
        square1->GetComponent<TransformComponent>().Position -= glm::vec3{ 0.5, 0.5, 0.0 };

        m_RenderSystem->SetFrameBuffer(m_FrameBuffer);

        m_Scene->AddSyetem(m_RenderSystem);
    }

    void EditorLayer::OnUpdate() {
        m_EditorCamera->OnUpdate();
        m_RenderSystem->RenderSingleCamera(*m_Scene, *m_EditorCamera);
        m_Scene->Update();
    }

    void EditorLayer::OnGuiRender() {
        OnDockingPanel();
        OnScenePanel();
        m_SceneHierarchy->OnGuiRender();
        m_Inspector->OnGuiRender();
        m_Concole->OnGuiRender();
        // ImGui::ShowDemoWindow();
    }

    void EditorLayer::OnDockingPanel() {
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
        ImGui::Begin("Hua Engine", &enable_docking, window_flags);
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
    }

    void EditorLayer::OnScenePanel() {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
        ImGui::Begin("Scene");
        ImVec2 scenePanelSize = ImGui::GetContentRegionAvail();
        if (glm::vec2{ scenePanelSize.x, scenePanelSize.y } != m_SceneViewportSize)
            m_FrameBuffer->Resize((uint32_t)scenePanelSize.x, (uint32_t)scenePanelSize.y);
        m_SceneViewportSize = { scenePanelSize.x, scenePanelSize.y };
        ImGui::Image(m_FrameBuffer->GetColorAttachment(),
            { m_SceneViewportSize.x , m_SceneViewportSize.y },
            { 0, 1 }, { 1, 0 });
        m_EditorCamera->SetViewport(m_SceneViewportSize.x, m_SceneViewportSize.y);
        ImGui::End();
        ImGui::PopStyleVar();
    }
}