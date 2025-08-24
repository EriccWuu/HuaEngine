#pragma once

#include "HuaEngine.h"
#include "Panels/SceneHierarchyPanel.h"
#include "Panels/InspectorPanel.h"
#include "Panels/ConsolePanel.h"

namespace HE {
    class EditorLayer : public HE::Layer {
    public:
        EditorLayer();

        void OnAttach() override;
        void OnUpdate() override;
        void OnGuiRender() override;

    private:
        void OnDockingPanel();
        void OnScenePanel();

    private:
        Ref<Shader> m_SquareShader;
        Ref<VertexArray> m_SquareVA;
        Ref<Texture2D> m_Texture;
        Ref<FrameBuffer> m_FrameBuffer;
        Ref<Rendering::EditorCamera> m_EditorCamera;
        Ref<Entity> m_Square, m_SceneCamera;
        Ref<Scene> m_Scene;
        Ref<RenderSystem> m_RenderSystem;
        Ref<SceneHierarchyPanel> m_SceneHierarchy;
        Ref<InspectorPanel> m_Inspector;
        Ref<ConcolePanel> m_Concole;

        glm::vec2 m_SceneViewportSize = { 0, 0 };

        TransformComponent trans;
    };
}