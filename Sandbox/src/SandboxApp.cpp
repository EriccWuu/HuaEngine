#include <iostream>
#include <HuaEngine.h>

class SandboxApp : public HE::Application {
public:
	SandboxApp() {

	}

	~SandboxApp() {

	}
};

HE::Application* HE::CreateApplication() {
	return new SandboxApp();
}