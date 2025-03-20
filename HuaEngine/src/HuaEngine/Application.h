#pragma once
#include "Core/Core.h"
#include "HuaEngine/Core/Window.h"
#include "Core/LayerStack.h"
#include "GUI/ImguiLayer.h"

namespace HE
{
	class ENGINE_API Application
	{
	public :
		Application();

		~Application();

		static Application& GetInstance() { return *ms_Instance; }
		Window& GetWindow() { return *m_Window; }

		void OnEvent(Event& e);

		void Run();
		bool OnWindowClose(Event& e);

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* layer);

	private:
		std::unique_ptr<Window> m_Window;
		ImguiLayer* m_GuiLayer;
		bool m_Running = true;
		LayerStack m_LayerStack;
		static Application* ms_Instance;
	};

	Application* CreateApplication();
}

