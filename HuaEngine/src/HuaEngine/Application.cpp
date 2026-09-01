#include "enginepch.h"
#include "Application.h"

#include <utility>

#include "Events/KeyEvent.h"
#include "Events/ApplicationEvent.h"
#include "HuaEngine/Application/ApplicationOperations.h"
#include "HuaEngine/Application/ApplicationServices.h"
#include "Serialization/Serialization.h"

namespace HE
{
	Application* Application::ms_Instance = nullptr;

	Application::Application(ApplicationSpecification specification)
		: m_Specification(std::move(specification))
	{
		HE_CORE_ASSERT(!ms_Instance, "There is already an exesisting Application instance!");
		ms_Instance = this;
	}

	Application::~Application()
	{
	}

	ApplicationOperations& Application::GetOperations()
	{
		HE_CORE_ASSERT(m_Operations, "Application operations are not registered yet");
		return *m_Operations;
	}

	const ApplicationOperations& Application::GetOperations() const
	{
		HE_CORE_ASSERT(m_Operations, "Application operations are not registered yet");
		return *m_Operations;
	}

	ApplicationServices& Application::GetServices()
	{
		HE_CORE_ASSERT(m_Services, "Runtime services are not registered yet");
		return *m_Services;
	}

	const ApplicationServices& Application::GetServices() const
	{
		HE_CORE_ASSERT(m_Services, "Runtime services are not registered yet");
		return *m_Services;
	}

	void Application::Start()
	{
		if (m_RuntimeInitialized) {
			return;
		}

		HE::Serialization::InitializeSerialization();

		if (m_Specification.EnableGuiLayer) {
			HE_CORE_ASSERT(m_Specification.EnableWindow, "GUI hosts must enable a runtime window");
		}

		if (m_Specification.EnableWindow) {
			m_Window = std::unique_ptr<Window>(Window::Create(WindowProps(
				m_Specification.Name,
				m_Specification.WindowWidth,
				m_Specification.WindowHeight)));
			m_Window->SetEventCallback(BIND_EVENT_FUNC(Application::OnEvent));
			m_Window->SetInputCallback([this](RawInputEvent event) { m_InputSystem.Submit(std::move(event)); });
			m_Window->SetFocusLostCallback([this]() { m_InputSystem.HandleFocusLost(); });
		}

		m_Services = CreateScope<ApplicationServices>();
		RegisterRuntimeServices(*m_Services);
		m_Operations.reset(new ApplicationOperations(*m_Services));

		if (m_Specification.EnableGuiLayer) {
			m_GuiLayer = new ImguiLayer();
			m_LayerStack.PushOverlay(m_GuiLayer);
			m_GuiLayer->OnAttach();
		}

		m_RuntimeInitialized = true;
		AttachDeferredLayers();
		OnRuntimeInitialized(*m_Operations);
	}

	void Application::AttachDeferredLayers()
	{
		for (Layer* layer : m_LayerStack) {
			if (layer == m_GuiLayer) {
				continue;
			}

			layer->OnAttach();
		}
	}

	void Application::OnEvent(Event& e) {
		for (auto it = m_LayerStack.End(); it != m_LayerStack.Begin();) {
			(*--it)->OnEvent(e);
			if (e.Handled)
				break;
		}
		if (!e.Handled) {
			auto dispatcher = EventDispatcher(e);
			dispatcher.Dispatch<WindowCloseEvent>(BIND_EVENT_FUNC(Application::OnWindowClose));
		}
	}

	void Application::Run()
	{
		Start();
		HE_CORE_ASSERT(m_Window, "Application::Run requires a window-enabled host");

		while (m_Running)
		{
			m_InputSystem.BeginFrame();
			m_Window->PollEvents();
			if (m_GuiLayer) {
				m_GuiLayer->Begin(&m_InputSystem);
			}
			(void)m_InputSystem.FinalizeFrame();

			for (auto layer : m_LayerStack) {
				layer->OnUpdate();
			}

			if (m_GuiLayer) {
				for (auto layer : m_LayerStack) {
					layer->OnGuiRender();
				}
				m_GuiLayer->End();
			}

			m_Window->Present();
		}
	}

	void Application::RequestShutdown()
	{
		m_Running = false;
	}

	bool Application::OnWindowClose(Event& e) {
		m_Running = false;
		return true;
	}

	void Application::PushLayer(Layer* layer) {
		m_LayerStack.PushLayer(layer);
		if (m_RuntimeInitialized) {
			layer->OnAttach();
		}
	}

	void Application::PushOverlay(Layer* layer) {
		m_LayerStack.PushOverlay(layer);
		if (m_RuntimeInitialized) {
			layer->OnAttach();
		}
	}
}
