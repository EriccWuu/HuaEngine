#include <iostream>
#include <HuaEngine.h>

class SandboxApp : public HE::Application {
public:
	SandboxApp() {
		PushLayer(new HE::ImguiLayer());
	}

	~SandboxApp() {

	}
};

HE::Application* HE::CreateApplication() {
	return new SandboxApp();
}