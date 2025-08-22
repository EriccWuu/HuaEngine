#include "enginepch.h"
#include "EditorLayer.h"

// Entry Point - Must be included in main application file only
#include "HuaEngine/EntryPoint.h"

namespace HE {
	class EditorApp : public Application {
	public:
		EditorApp() {
			PushLayer(new EditorLayer());
		}

		~EditorApp() {

		}
	};

	HE::Application* HE::CreateApplication() {
		return new EditorApp();
	}
}