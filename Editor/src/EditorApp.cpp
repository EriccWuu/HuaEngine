#include "enginepch.h"
#include "HuaEngine/EntryPoint.h"  
#include "EditorLayer.h"

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