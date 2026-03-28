#pragma once

#include <memory>
#include <string>

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Core/Window.h"
#include "HuaEngine/Core/LayerStack.h"
#include "HuaEngine/Events/Event.h"
#include "HuaEngine/GUI/ImguiLayer.h"

namespace HE
{
	class ApplicationServices;
	class ApplicationOperations;

	struct CommandLineArguments {
		int Count = 0;
		char** Values = nullptr;

		const char* operator[](int index) const {
			if (index < 0 || index >= Count || Values == nullptr) {
				return nullptr;
			}

			return Values[index];
		}
	};

	struct ApplicationSpecification {
		std::string Name = "HuaEngine";
		bool EnableWindow = true;
		bool EnableGuiLayer = true;
		uint32_t WindowWidth = 1960;
		uint32_t WindowHeight = 1080;
		CommandLineArguments CommandLineArgs;
	};

	class ENGINE_API Application
	{
	public :
		explicit Application(ApplicationSpecification specification = {});

		virtual ~Application();

		static Application& GetInstance() { return *ms_Instance; }
		Window& GetWindow() {
			HE_CORE_ASSERT(m_Window, "Application window is not available for this host");
			return *m_Window;
		}
		const ApplicationSpecification& GetSpecification() const { return m_Specification; }
		bool IsRuntimeInitialized() const { return m_RuntimeInitialized; }
		ApplicationOperations& GetOperations();
		const ApplicationOperations& GetOperations() const;

		void Start();
		void OnEvent(Event& e);

		void Run();
		void RequestShutdown();
		bool OnWindowClose(Event& e);

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* layer);

	protected:
		ApplicationServices& GetServices();
		const ApplicationServices& GetServices() const;

		virtual void RegisterRuntimeServices(ApplicationServices& services) { (void)services; }
		virtual void OnRuntimeInitialized(ApplicationOperations& operations) { (void)operations; }

	private:
		void AttachDeferredLayers();

		ApplicationSpecification m_Specification;
		std::unique_ptr<Window> m_Window;
		ImguiLayer* m_GuiLayer = nullptr;
		bool m_Running = true;
		bool m_RuntimeInitialized = false;
		Scope<ApplicationServices> m_Services;
		Scope<ApplicationOperations> m_Operations;
		LayerStack m_LayerStack;
		static Application* ms_Instance;
	};

	Application* CreateApplication(CommandLineArguments args = {});
}
