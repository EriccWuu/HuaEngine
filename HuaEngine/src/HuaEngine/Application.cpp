#include "enginepch.h"
#include "Application.h"
#include "Events/KeyEvent.h"
#include "Events/ApplicationEvent.h"
#include "Serialization/Serialization.h"

namespace HE
{
	Application* Application::ms_Instance = nullptr;

	Application::Application() 
	{
		HE_CORE_ASSERT(!ms_Instance, "There is already an exesisting Application instance!");
		ms_Instance = this;

		// Initialize serialization system
		HE::Serialization::InitializeSerialization();

		m_Window = std::unique_ptr<Window>(Window::Create());
		m_Window->SetEventCallback(BIND_EVENT_FUNC(Application::OnEvent));

		m_GuiLayer = new ImguiLayer();
		PushOverlay(m_GuiLayer);
	}

	Application::~Application()
	{
	}

	void Application::OnEvent(Event& e) {
		auto dispatcher = EventDispatcher(e);
		dispatcher.Dispatch<WindowCloseEvent>(BIND_EVENT_FUNC(Application::OnWindowClose));

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

			m_GuiLayer->Begin();
			for (auto layer : m_LayerStack) {
				layer->OnGuiRender();
			}
			m_GuiLayer->End();

			m_Window->OnUpdate();
		}
	}

	bool Application::OnWindowClose(Event& e) {
		m_Running = false;
		return true;
	}

	void Application::PushLayer(Layer* layer) {
		m_LayerStack.PushLayer(layer);
		layer->OnAttach();
	}

	void Application::PushOverlay(Layer* layer) {
		m_LayerStack.PushOverlay(layer);
		layer->OnAttach();
	}
}