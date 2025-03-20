#pragma once

#include "HuaEngine/Core/Layer.h"
#include "HuaEngine/Events/ApplicationEvent.h"
#include "HuaEngine/Events/KeyEvent.h"
#include "HuaEngine/Events/MouseEvent.h"

namespace HE {
	class ENGINE_API ImguiLayer : public Layer {
	public:
		ImguiLayer(): Layer("ImGuiLayer") {}
		~ImguiLayer() {}

		void OnUpdate() override;
		void OnEvent(Event& e) override;
		void OnAttach() override;
		void OnDetach() override;
	private:
		float m_Time = 0.f;

		bool OnMouseButtonPressed(MouseButtonPressedEvent& e);
		bool OnMouseButtonReleased(MouseButtonReleasedEvent& e);
		bool OnMouseMoved(MouseMovedEvent& e);
		bool OnMouseScrolled(MouseScrolledEvent& e);
		bool OnKeyPressed(KeyPressedEvent& e);
		bool OnKeyReleased(KeyReleasedEvent& e);
		bool OnKeyTyped(KeyTypedEvent& e);
		bool OnWindowResized(WindowResizeEvent& e);
	};
}