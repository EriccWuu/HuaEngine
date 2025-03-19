#include "enginepch.h"
#include "Application.h"
#include "Events/KeyEvent.h"
#include "Events/ApplicationEvent.h"


namespace HE
{
#define BIND_EVENT_FUNC(x) std::bind(&x, this, std::placeholders::_1)
	Application::Application() 
	{
		m_Window = std::unique_ptr<Window>(Window::Create());
		m_Window->SetEventCallback(BIND_EVENT_FUNC(Application::OnEvent));
	}

	Application::~Application()
	{
	}

	void Application::OnEvent(Event& e) {
		auto dispatcher = EventDispatcher(e);
		dispatcher.Dispatch<WindowCloseEvent>(BIND_EVENT_FUNC(Application::OnWindowClose));
		HE_CORE_INFO("{0}", e.ToString());

		for (auto it = m_LayerStack.End(); it != m_LayerStack.Begin();) {
			(*--it)->OnEvent(e);
			if (e.Handled)
				break;
		}
	}

	void Application::Run()
	{
		while (m_Running)
		{
			for (auto layer : m_LayerStack) {
				layer->OnUpdate();
			}
			m_Window->OnUpdate();
		}
	}

	bool Application::OnWindowClose(Event& e) {
		m_Running = false;
		return true;
	}

	void Application::PushLayer(Layer* layer) {
		m_LayerStack.PushLayer(layer);
	}

	void Application::PopLayer(Layer* layer) {
		m_LayerStack.PopLayer(layer);
	}
}