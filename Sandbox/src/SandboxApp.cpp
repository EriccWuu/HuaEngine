#include <iostream>
#include <HuaEngine.h>

class MyLayer : public HE::Layer {
public:
	MyLayer(const std::string& name = "MyLayer") : Layer(name) {}
	void OnEvent(HE::Event& e) override {
		HE_INFO("MyLayer: {0}", e.ToString());
	}

	void OnUpdate() override {
		// HE_INFO("MyLayer Update");
	}
};

class SandboxApp : public HE::Application {
public:
	SandboxApp() {
		HE::Layer* myLayer = new MyLayer();
		PushLayer(myLayer);
	}

	~SandboxApp() {

	}
};

HE::Application* HE::CreateApplication() {
	return new SandboxApp();
}