#include "enginepch.h"
#include "EditorLayer.h"

// Entry Point - Must be included in main application file only
#include "HuaEngine/EntryPoint.h"

namespace HE {
	class EditorApp : public Application {
	public:
		EditorApp()
			: Application(ApplicationSpecification{
				.Name = "HuaEditor",
				.EnableGuiLayer = true
			}) {
			EditorLayerSpecification layerSpecification;
			layerSpecification.BootstrapDemoScene = true;
			PushLayer(new EditorLayer(layerSpecification));
		}

		~EditorApp() {

		}
	};

	HE::Application* HE::CreateApplication() {
		return new EditorApp();
	}
}
