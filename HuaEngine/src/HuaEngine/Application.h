#pragma once
#include "Core/Core.h"
#include "HuaEngine/Window.h"
#include "Core/LayerStack.h"

namespace HE
{
	class ENGINE_API Application
	{
	public :
		Application();

		~Application();

		void OnEvent(Event& e);

		void Run();
		bool OnWindowClose(Event& e);

		void PushLayer(Layer* layer);
		void PopLayer(Layer* layer);

	private:
		std::unique_ptr<Window> m_Window;
		bool m_Running = true;
		LayerStack m_LayerStack;
	};

	Application* CreateApplication();
}

